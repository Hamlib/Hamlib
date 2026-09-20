/*
 * Hamlib cache snapshot concurrency tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <pthread.h>
#include <sched.h>
#include <stdio.h>

#include "hamlib/rig.h"
#include "hamlib/riglist.h"
#include "hamlib/rig_state.h"
#include "cache.h"
#include "misc.h"

#define ITERATION_COUNT 50000
#define INVALID_CACHE_AGE_MS 10000

struct start_gate
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int waiting;
    int released;
};

struct worker_args
{
    RIG *rig;
    struct start_gate *gate;
    int failed;
};

static int check_status(const char *operation, int status)
{
    if (status == RIG_OK)
    {
        return 0;
    }

    fprintf(stderr, "%s failed: %s\n", operation, rigerror(status));
    return 1;
}

static int start_gate_init(struct start_gate *gate)
{
    int status = pthread_mutex_init(&gate->mutex, NULL);

    if (status != 0)
    {
        return status;
    }

    status = pthread_cond_init(&gate->cond, NULL);

    if (status != 0)
    {
        pthread_mutex_destroy(&gate->mutex);
        return status;
    }

    gate->waiting = 0;
    gate->released = 0;
    return 0;
}

static void start_gate_wait(struct start_gate *gate)
{
    pthread_mutex_lock(&gate->mutex);
    gate->waiting++;
    pthread_cond_broadcast(&gate->cond);

    while (!gate->released)
    {
        pthread_cond_wait(&gate->cond, &gate->mutex);
    }

    pthread_mutex_unlock(&gate->mutex);
}

static void start_gate_release(struct start_gate *gate)
{
    pthread_mutex_lock(&gate->mutex);

    while (gate->waiting < 2)
    {
        pthread_cond_wait(&gate->cond, &gate->mutex);
    }

    gate->released = 1;
    pthread_cond_broadcast(&gate->cond);
    pthread_mutex_unlock(&gate->mutex);
}

static void start_gate_release_early(struct start_gate *gate)
{
    pthread_mutex_lock(&gate->mutex);
    gate->released = 1;
    pthread_cond_broadcast(&gate->cond);
    pthread_mutex_unlock(&gate->mutex);
}

static void start_gate_destroy(struct start_gate *gate)
{
    pthread_cond_destroy(&gate->cond);
    pthread_mutex_destroy(&gate->mutex);
}

static void yield_periodically(int iteration)
{
    if ((iteration & 63) == 0)
    {
        sched_yield();
    }
}

static void *write_vfo_tuple(void *arg)
{
    struct worker_args *args = (struct worker_args *)arg;
    int i;

    start_gate_wait(args->gate);

    for (i = 0; i < ITERATION_COUNT; i++)
    {
        vfo_t vfo = (i & 1) ? RIG_VFO_B : RIG_VFO_A;

        rig_set_vfo_state(args->rig, vfo, vfo);
        yield_periodically(i);
    }

    return NULL;
}

static void *read_vfo_tuple(void *arg)
{
    struct worker_args *args = (struct worker_args *)arg;
    int i;

    start_gate_wait(args->gate);

    for (i = 0; i < ITERATION_COUNT; i++)
    {
        struct rig_cache_routing_snapshot snapshot;

        rig_get_cache_routing_snapshot(args->rig, &snapshot);

        if (!((snapshot.current_vfo == RIG_VFO_A
                && snapshot.tx_vfo == RIG_VFO_A)
                || (snapshot.current_vfo == RIG_VFO_B
                    && snapshot.tx_vfo == RIG_VFO_B)))
        {
            fprintf(stderr, "torn VFO tuple: current=%s, tx=%s\n",
                    rig_strvfo(snapshot.current_vfo),
                    rig_strvfo(snapshot.tx_vfo));
            args->failed = 1;
            break;
        }

        yield_periodically(i);
    }

    return NULL;
}

static void *write_split_tuple(void *arg)
{
    struct worker_args *args = (struct worker_args *)arg;
    int i;

    start_gate_wait(args->gate);

    for (i = 0; i < ITERATION_COUNT; i++)
    {
        if (i & 1)
        {
            rig_set_split_routing_state(args->rig, RIG_SPLIT_ON,
                                        RIG_VFO_B, RIG_VFO_B);
        }
        else
        {
            rig_set_split_routing_state(args->rig, RIG_SPLIT_OFF,
                                        RIG_VFO_A, RIG_VFO_A);
        }

        yield_periodically(i);
    }

    return NULL;
}

static void *read_split_tuple(void *arg)
{
    struct worker_args *args = (struct worker_args *)arg;
    int i;

    start_gate_wait(args->gate);

    for (i = 0; i < ITERATION_COUNT; i++)
    {
        struct rig_cache_routing_snapshot snapshot;
        int split_off;
        int split_on;

        rig_get_cache_routing_snapshot(args->rig, &snapshot);
        split_off = snapshot.split == RIG_SPLIT_OFF
                    && snapshot.rx_vfo == RIG_VFO_A
                    && snapshot.tx_vfo == RIG_VFO_A
                    && snapshot.split_vfo == RIG_VFO_A;
        split_on = snapshot.split == RIG_SPLIT_ON
                   && snapshot.rx_vfo == RIG_VFO_B
                   && snapshot.tx_vfo == RIG_VFO_B
                   && snapshot.split_vfo == RIG_VFO_B;

        if (!split_off && !split_on)
        {
            fprintf(stderr,
                    "torn split tuple: split=%d, rx=%s, tx=%s, split_vfo=%s\n",
                    snapshot.split, rig_strvfo(snapshot.rx_vfo),
                    rig_strvfo(snapshot.tx_vfo),
                    rig_strvfo(snapshot.split_vfo));
            args->failed = 1;
            break;
        }

        yield_periodically(i);
    }

    return NULL;
}

static void *write_cache_tuple(void *arg)
{
    struct worker_args *args = (struct worker_args *)arg;
    int i;

    start_gate_wait(args->gate);

    for (i = 0; i < ITERATION_COUNT; i++)
    {
        rmode_t mode = (i & 1) ? RIG_MODE_LSB : RIG_MODE_USB;
        pbwidth_t width = (i & 1) ? 2400 : 2100;
        freq_t freq = (i & 1) ? 7074000 : 14074000;

        if (rig_set_cache_mode(args->rig, RIG_VFO_A, mode, width) != RIG_OK
                || rig_set_cache_freq(args->rig, RIG_VFO_A, freq) != RIG_OK)
        {
            args->failed = 1;
            break;
        }

        rig_set_cache_ptt(args->rig, (i & 1) ? RIG_PTT_ON : RIG_PTT_OFF);
        rig_set_cache_satmode(args->rig, i & 1);
        yield_periodically(i);
    }

    return NULL;
}

static void *read_cache_tuple(void *arg)
{
    struct worker_args *args = (struct worker_args *)arg;
    int i;

    start_gate_wait(args->gate);

    for (i = 0; i < ITERATION_COUNT; i++)
    {
        struct rig_cache_snapshot snapshot;
        int usb;
        int lsb;

        rig_get_cache_snapshot(args->rig, &snapshot);
        usb = snapshot.modeMainA == RIG_MODE_USB
              && snapshot.widthMainA == 2100;
        lsb = snapshot.modeMainA == RIG_MODE_LSB
              && snapshot.widthMainA == 2400;

        if ((!usb && !lsb)
                || (snapshot.freqMainA != 14074000
                    && snapshot.freqMainA != 7074000)
                || (snapshot.ptt != RIG_PTT_OFF
                    && snapshot.ptt != RIG_PTT_ON)
                || (snapshot.satmode != 0 && snapshot.satmode != 1))
        {
            fprintf(stderr,
                    "invalid cache snapshot: freq=%.0f, mode=%s, width=%d, "
                    "ptt=%d, satmode=%d\n",
                    snapshot.freqMainA, rig_strrmode(snapshot.modeMainA),
                    (int)snapshot.widthMainA, snapshot.ptt,
                    snapshot.satmode);
            args->failed = 1;
            break;
        }

        yield_periodically(i);
    }

    return NULL;
}

static int run_workers(RIG *rig, const char *name,
                       void *(*writer)(void *), void *(*reader)(void *))
{
    struct start_gate gate;
    struct worker_args writer_args = { .rig = rig, .gate = &gate };
    struct worker_args reader_args = { .rig = rig, .gate = &gate };
    pthread_t writer_thread;
    pthread_t reader_thread;
    int writer_created = 0;
    int reader_created = 0;
    int status;

    status = start_gate_init(&gate);

    if (status != 0)
    {
        fprintf(stderr, "%s gate initialization failed: %d\n", name, status);
        return 1;
    }

    status = pthread_create(&writer_thread, NULL, writer, &writer_args);

    if (status == 0)
    {
        writer_created = 1;
        status = pthread_create(&reader_thread, NULL, reader, &reader_args);

        if (status == 0)
        {
            reader_created = 1;
            start_gate_release(&gate);
        }
    }

    if (!writer_created || !reader_created)
    {
        fprintf(stderr, "%s thread creation failed: %d\n", name, status);
        start_gate_release_early(&gate);
    }

    if (writer_created)
    {
        pthread_join(writer_thread, NULL);
    }

    if (reader_created)
    {
        pthread_join(reader_thread, NULL);
    }

    start_gate_destroy(&gate);
    return !writer_created || !reader_created
           || writer_args.failed || reader_args.failed;
}

static int check_snapshot_values(RIG *rig)
{
    struct rig_cache_snapshot snapshot;

    rig_observe_current_vfo(rig, RIG_VFO_B);
    rig_set_vfo_state(rig, RIG_VFO_A, RIG_VFO_B);
    rig_set_split_routing_state(rig, RIG_SPLIT_ON,
                                RIG_VFO_A, RIG_VFO_B);
    rig_set_cache_ptt(rig, RIG_PTT_ON);
    rig_set_cache_satmode(rig, 1);

    if (check_status("set VFO A frequency",
                     rig_set_cache_freq(rig, RIG_VFO_A, 14074000))
            || check_status("set VFO B frequency",
                            rig_set_cache_freq(rig, RIG_VFO_B, 7074000))
            || check_status("set VFO A mode",
                            rig_set_cache_mode(rig, RIG_VFO_A,
                                    RIG_MODE_USB, 2100))
            || check_status("set VFO B mode",
                            rig_set_cache_mode(rig, RIG_VFO_B,
                                    RIG_MODE_LSB, 2400))
            || check_status("set VFO C frequency",
                            rig_set_cache_freq(rig, RIG_VFO_C, 10136000))
            || check_status("set VFO C mode",
                            rig_set_cache_mode(rig, RIG_VFO_C,
                                    RIG_MODE_CW, 500))
            || check_status("set sub-A frequency",
                            rig_set_cache_freq(rig, RIG_VFO_SUB_A, 144174000))
            || check_status("set sub-A mode",
                            rig_set_cache_mode(rig, RIG_VFO_SUB_A,
                                    RIG_MODE_PKTUSB, 3000))
            || check_status("set sub-B frequency",
                            rig_set_cache_freq(rig, RIG_VFO_SUB_B, 432174000))
            || check_status("set sub-B mode",
                            rig_set_cache_mode(rig, RIG_VFO_SUB_B,
                                    RIG_MODE_FM, 12000))
            || check_status("set sub-C frequency",
                            rig_set_cache_freq(rig, RIG_VFO_SUB_C, 1296174000))
            || check_status("set sub-C mode",
                            rig_set_cache_mode(rig, RIG_VFO_SUB_C,
                                    RIG_MODE_AM, 6000))
            || check_status("set memory frequency",
                            rig_set_cache_freq(rig, RIG_VFO_MEM, 146520000))
            || check_status("set memory mode",
                            rig_set_cache_mode(rig, RIG_VFO_MEM,
                                    RIG_MODE_FM, 15000)))
    {
        return 1;
    }

    rig_get_cache_snapshot(rig, &snapshot);

    if (snapshot.current_vfo != RIG_VFO_A
            || snapshot.observed_vfo != RIG_VFO_B
            || snapshot.rx_vfo != RIG_VFO_A
            || snapshot.tx_vfo != RIG_VFO_B
            || snapshot.freqCurr != 14074000
            || snapshot.freqOther != 0
            || snapshot.freqMainA != 14074000
            || snapshot.freqMainB != 7074000
            || snapshot.freqMainC != 10136000
            || snapshot.freqSubA != 144174000
            || snapshot.freqSubB != 432174000
            || snapshot.freqSubC != 1296174000
            || snapshot.freqMem != 146520000
            || snapshot.modeCurr != RIG_MODE_USB
            || snapshot.modeOther != RIG_MODE_NONE
            || snapshot.modeMainA != RIG_MODE_USB
            || snapshot.modeMainB != RIG_MODE_LSB
            || snapshot.modeMainC != RIG_MODE_CW
            || snapshot.modeSubA != RIG_MODE_PKTUSB
            || snapshot.modeSubB != RIG_MODE_FM
            || snapshot.modeSubC != RIG_MODE_AM
            || snapshot.modeMem != RIG_MODE_FM
            || snapshot.widthCurr != 2100
            || snapshot.widthOther != 0
            || snapshot.widthMainA != 2100
            || snapshot.widthMainB != 2400
            || snapshot.widthMainC != 500
            || snapshot.widthSubA != 3000
            || snapshot.widthSubB != 12000
            || snapshot.widthSubC != 6000
            || snapshot.widthMem != 15000
            || snapshot.ptt != RIG_PTT_ON
            || snapshot.split != RIG_SPLIT_ON
            || snapshot.split_vfo != RIG_VFO_B
            || snapshot.satmode != 1)
    {
        fprintf(stderr, "full cache snapshot did not preserve seeded values\n");
        return 1;
    }

    return 0;
}

static int check_current_frequency_invalidation(RIG *rig)
{
    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
    int freq_age;
    int mode_age;
    int width_age;

    rig_set_vfo_state(rig, RIG_VFO_A, RIG_VFO_A);

    if (check_status("set current frequency",
                     rig_set_cache_freq(rig, RIG_VFO_A, 14074000))
            || check_status("get current frequency before invalidation",
                            rig_get_cache(rig, RIG_VFO_A, &freq, &freq_age,
                                          &mode, &mode_age, &width, &width_age)))
    {
        return 1;
    }

    if (freq != 14074000 || freq_age < 0
            || freq_age >= INVALID_CACHE_AGE_MS)
    {
        fprintf(stderr,
                "current frequency cache was not valid: freq=%.0f, age=%dms\n",
                freq, freq_age);
        return 1;
    }

    rig_invalidate_cache_current_freq(rig);

    if (check_status("get current frequency after invalidation",
                     rig_get_cache(rig, RIG_VFO_A, &freq, &freq_age,
                                   &mode, &mode_age, &width, &width_age)))
    {
        return 1;
    }

    if (freq != 14074000 || freq_age < INVALID_CACHE_AGE_MS)
    {
        fprintf(stderr,
                "current frequency invalidation mismatch: freq=%.0f, "
                "age=%dms\n", freq, freq_age);
        return 1;
    }

    return 0;
}

int main(void)
{
    RIG *rig;
    int result = 1;

    rig_set_debug(RIG_DEBUG_ERR);

    if (check_status("rig_load_backend", rig_load_backend("dummy")))
    {
        return 1;
    }

    rig = rig_init(RIG_MODEL_DUMMY);

    if (rig == NULL)
    {
        fprintf(stderr, "failed to initialize Dummy rig\n");
        return 1;
    }

    STATE(rig)->comm_state = 1;
    rig_set_vfo_state(rig, RIG_VFO_A, RIG_VFO_A);

    if (run_workers(rig, "VFO tuple", write_vfo_tuple, read_vfo_tuple))
    {
        goto cleanup;
    }

    rig_set_split_routing_state(rig, RIG_SPLIT_OFF,
                                RIG_VFO_A, RIG_VFO_A);

    if (run_workers(rig, "split tuple", write_split_tuple, read_split_tuple))
    {
        goto cleanup;
    }

    if (check_status("seed cache mode",
                     rig_set_cache_mode(rig, RIG_VFO_A,
                                        RIG_MODE_USB, 2100))
            || check_status("seed cache frequency",
                            rig_set_cache_freq(rig, RIG_VFO_A, 14074000)))
    {
        goto cleanup;
    }

    rig_set_cache_ptt(rig, RIG_PTT_OFF);
    rig_set_cache_satmode(rig, 0);

    if (run_workers(rig, "cache tuple", write_cache_tuple, read_cache_tuple)
            || check_snapshot_values(rig)
            || check_current_frequency_invalidation(rig))
    {
        goto cleanup;
    }

    result = 0;

cleanup:
    STATE(rig)->comm_state = 0;
    rig_cleanup(rig);
    return result;
}
