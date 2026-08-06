/*
 *  Hamlib Interface - event handling header
 *  Copyright (c) 2000-2003 by Stephane Fillod and Frank Singleton
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

#ifndef _EVENT_H
#define _EVENT_H 1

#include <stdint.h>

#include "hamlib/rig.h"

#define RIG_POLL_CHANGE_INTERVAL_MS 50

struct rig_poll_schedule
{
    int64_t next_change_ms;
    int64_t next_publish_ms;
    int poll_interval_ms;
};

struct rig_poll_schedule_result
{
    int scan_cache;
    int publish;
    int64_t next_deadline_ms;
};

static inline void rig_poll_schedule_init(struct rig_poll_schedule *schedule,
        int64_t now_ms, int poll_interval_ms)
{
    schedule->next_change_ms = now_ms;
    schedule->next_publish_ms = poll_interval_ms > 0 ? now_ms : 0;
    schedule->poll_interval_ms = poll_interval_ms;
}

static inline struct rig_poll_schedule_result rig_poll_schedule_advance(
    struct rig_poll_schedule *schedule, int64_t now_ms, int poll_interval_ms)
{
    struct rig_poll_schedule_result result = { 0, 0, 0 };

    if (poll_interval_ms != schedule->poll_interval_ms)
    {
        schedule->poll_interval_ms = poll_interval_ms;
        schedule->next_publish_ms = poll_interval_ms > 0
                                    ? now_ms + poll_interval_ms : 0;
    }

    if (now_ms >= schedule->next_change_ms)
    {
        result.scan_cache = 1;
        schedule->next_change_ms = now_ms + RIG_POLL_CHANGE_INTERVAL_MS;
    }

    if (schedule->poll_interval_ms > 0
            && now_ms >= schedule->next_publish_ms)
    {
        result.publish = 1;
        schedule->next_publish_ms = now_ms + poll_interval_ms;
    }

    result.next_deadline_ms = schedule->next_change_ms;

    if (schedule->poll_interval_ms > 0
            && schedule->next_publish_ms < result.next_deadline_ms)
    {
        result.next_deadline_ms = schedule->next_publish_ms;
    }

    return result;
}

static inline void rig_poll_schedule_published(
    struct rig_poll_schedule *schedule, int64_t now_ms)
{
    if (schedule->poll_interval_ms > 0)
    {
        schedule->next_publish_ms = now_ms + schedule->poll_interval_ms;
    }
}

int rig_poll_routine_start(RIG *rig);
int rig_poll_routine_stop(RIG *rig);
int rig_get_poll_interval(RIG *rig);
void rig_set_poll_interval(RIG *rig, int interval_ms);

int rig_fire_freq_event(RIG *rig, vfo_t vfo, freq_t freq);
int rig_fire_mode_event(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int rig_fire_vfo_event(RIG *rig, vfo_t vfo);
int rig_fire_ptt_event(RIG *rig, vfo_t vfo, ptt_t ptt);
int rig_fire_dcd_event(RIG *rig, vfo_t vfo, dcd_t dcd);
int rig_fire_pltune_event(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode, pbwidth_t *width);
int rig_fire_spectrum_event(RIG *rig, struct rig_spectrum_line *line);

#endif /* _EVENT_H */
