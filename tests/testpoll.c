/*
 * Deterministic tests for the rig poll scheduler.
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>

#include "event.h"

static void expect_result(const char *name,
                          struct rig_poll_schedule_result result,
                          int scan_cache, int publish,
                          int64_t next_deadline_ms)
{
    if (result.scan_cache != scan_cache || result.publish != publish
            || result.next_deadline_ms != next_deadline_ms)
    {
        fprintf(stderr,
                "%s: got scan=%d publish=%d deadline=%lld; "
                "expected scan=%d publish=%d deadline=%lld\n",
                name, result.scan_cache, result.publish,
                (long long) result.next_deadline_ms, scan_cache, publish,
                (long long) next_deadline_ms);
        exit(EXIT_FAILURE);
    }
}

static void test_short_intervals(void)
{
    static const int intervals[] = { 1, 17, 49 };
    size_t i;

    for (i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++)
    {
        struct rig_poll_schedule schedule;
        int interval = intervals[i];
        int64_t second_deadline = 100 + 2 * interval;

        if (second_deadline > 150)
        {
            second_deadline = 150;
        }

        rig_poll_schedule_init(&schedule, 100, interval);
        expect_result("short initial",
                      rig_poll_schedule_advance(&schedule, 100, interval),
                      1, 1, 100 + interval);
        expect_result("short heartbeat",
                      rig_poll_schedule_advance(&schedule,
                                                100 + interval, interval),
                      0, 1, second_deadline);
    }
}

static void test_nonmultiple_interval(void)
{
    struct rig_poll_schedule schedule;

    rig_poll_schedule_init(&schedule, 0, 75);
    expect_result("nonmultiple initial",
                  rig_poll_schedule_advance(&schedule, 0, 75),
                  1, 1, 50);
    expect_result("nonmultiple scan",
                  rig_poll_schedule_advance(&schedule, 50, 75),
                  1, 0, 75);
    expect_result("nonmultiple heartbeat",
                  rig_poll_schedule_advance(&schedule, 75, 75),
                  0, 1, 100);
}

static void test_no_catchup_burst(void)
{
    struct rig_poll_schedule schedule;

    rig_poll_schedule_init(&schedule, 0, 10);
    rig_poll_schedule_advance(&schedule, 0, 10);
    expect_result("late heartbeat",
                  rig_poll_schedule_advance(&schedule, 35, 10),
                  0, 1, 45);
    expect_result("no catchup",
                  rig_poll_schedule_advance(&schedule, 35, 10),
                  0, 0, 45);
}

static void test_runtime_disable_and_enable(void)
{
    struct rig_poll_schedule schedule;

    rig_poll_schedule_init(&schedule, 0, 20);
    rig_poll_schedule_advance(&schedule, 0, 20);
    expect_result("disabled heartbeat",
                  rig_poll_schedule_advance(&schedule, 20, 0),
                  0, 0, 50);
    expect_result("disabled scan",
                  rig_poll_schedule_advance(&schedule, 50, 0),
                  1, 0, 100);
    expect_result("enabled heartbeat pending",
                  rig_poll_schedule_advance(&schedule, 55, 7),
                  0, 0, 62);
    expect_result("enabled heartbeat",
                  rig_poll_schedule_advance(&schedule, 62, 7),
                  0, 1, 69);
}

static void test_change_publish_restarts_heartbeat(void)
{
    struct rig_poll_schedule schedule;

    rig_poll_schedule_init(&schedule, 0, 100);
    rig_poll_schedule_advance(&schedule, 0, 100);
    rig_poll_schedule_published(&schedule, 40);
    expect_result("change publish heartbeat pending",
                  rig_poll_schedule_advance(&schedule, 100, 100),
                  1, 0, 140);
    expect_result("change publish heartbeat",
                  rig_poll_schedule_advance(&schedule, 140, 100),
                  0, 1, 150);
}

int main(void)
{
    test_short_intervals();
    test_nonmultiple_interval();
    test_no_catchup_burst();
    test_runtime_disable_and_enable();
    test_change_publish_restarts_heartbeat();
    return EXIT_SUCCESS;
}
