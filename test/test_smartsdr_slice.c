/*
 *  Hamlib SmartSDR slice, VFO and split tests
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* A slice is a whole receiver, so this radio reaches split by putting a second
 * one on the air rather than by switching a VFO. Which slice each call lands
 * on is the thing worth checking, and it is only visible on the wire, so each
 * case drives the public API against the mock radio and reads back what the
 * radio was told. */

#include "acutest.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>

#include "smartsdr_mock.h"
#include "smartsdr_slice.h"

#define MOCK_TIMEOUT_MS   "300"
#define MOCK_LIVENESS_MS  "5000"


static RIG *open_on_mock(struct smartsdr_mock *m)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR);
    char path[64];

    if (!rig)
    {
        TEST_ASSERT(0 && "rig_init failed");
    }

    smartsdr_mock_pathname(m, path, sizeof(path));
    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), path);
    rig_set_conf(rig, rig_token_lookup(rig, "timeout"), MOCK_TIMEOUT_MS);
    rig_set_conf(rig, rig_token_lookup(rig, "retry"), "0");
    rig_set_conf(rig, rig_token_lookup(rig, "timeout_retry"), "0");
    rig_set_conf(rig, rig_token_lookup(rig, "liveness_timeout"),
                 MOCK_LIVENESS_MS);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    return rig;
}


static void close_on_mock(RIG *rig, struct smartsdr_mock *m)
{
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(m);
}


/* ================================================================== */
/* Which VFO names mean the second receiver                            */
/* ================================================================== */

/* The radio letters its slices A-H, so Main and Sub name the two receivers.
 * A and B are accepted as the names an application is most likely to use, and
 * TX means whichever slice holds the transmitter. Anything else is refused
 * rather than silently treated as the first receiver. */
void test_vfo_names_map_to_the_right_receiver(void)
{
    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_MAIN) == 0);
    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_MAIN_A) == 0);
    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_A) == 0);
    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_CURR) == 0);

    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_SUB) == 1);
    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_SUB_A) == 1);
    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_B) == 1);
    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_TX) == 1);

    TEST_CHECK(smartsdr_vfo_is_sub(RIG_VFO_C) < 0);
    TEST_MSG("a VFO this radio has no receiver for must be refused");
}


/* ================================================================== */
/* Selecting a receiver                                                */
/* ================================================================== */

/* The mock radio has one slice, so there is no second receiver to select and
 * saying so beats addressing the first one by accident. */
void test_selecting_sub_without_a_second_slice_is_refused(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    TEST_CHECK(rig_set_vfo(rig, RIG_VFO_SUB) == -RIG_ENAVAIL);

    close_on_mock(rig, &m);
}


void test_reading_the_vfo_reports_main_with_one_slice(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    vfo_t vfo = RIG_VFO_NONE;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);

    TEST_CHECK(rig_get_vfo(rig, &vfo) == RIG_OK);
    TEST_CHECK(vfo == RIG_VFO_MAIN);
    TEST_MSG("with no active second receiver the answer is Main");

    close_on_mock(rig, &m);
}


/* ================================================================== */
/* Split                                                               */
/* ================================================================== */

/* Turning split on puts a second slice on the air: the radio is asked for one
 * and the transmitter is moved to it. Setting tx=1 anywhere clears it
 * everywhere else, so the first slice needs no separate command. */
void test_split_on_creates_a_slice_and_gives_it_the_transmitter(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "slice create", "1");

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    TEST_CHECK(rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON,
                                 RIG_VFO_SUB) == RIG_OK);

    TEST_CHECK(smartsdr_mock_saw(&m, "slice create"));
    TEST_MSG("split needs a second receiver to transmit on");
    TEST_CHECK(smartsdr_mock_saw(&m, "slice set 1 tx=1"));
    TEST_MSG("the transmitter must move to the slice that was created");

    close_on_mock(rig, &m);
}


/* Asking twice must not leave a second slice behind on the radio. */
void test_split_on_twice_creates_only_one_slice(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "slice create", "1");

    rig = open_on_mock(&m);
    rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, RIG_VFO_SUB);
    smartsdr_mock_forget(&m);

    TEST_CHECK(rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON,
                                 RIG_VFO_SUB) == RIG_OK);
    TEST_CHECK(!smartsdr_mock_saw(&m, "slice create"));
    TEST_MSG("already split, so nothing more should be asked of the radio");

    close_on_mock(rig, &m);
}


/* Turning split off brings the transmitter home and hands back the slice this
 * backend borrowed, rather than leaving the operator an extra receiver. */
void test_split_off_returns_the_transmitter_and_the_slice(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "slice create", "1");

    rig = open_on_mock(&m);
    rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, RIG_VFO_SUB);
    smartsdr_mock_forget(&m);

    TEST_CHECK(rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_OFF,
                                 RIG_VFO_CURR) == RIG_OK);

    TEST_CHECK(smartsdr_mock_saw(&m, "slice set 0 tx=1"));
    TEST_MSG("the transmitter belongs back on this rig's own slice");
    TEST_CHECK(smartsdr_mock_saw(&m, "slice remove 1"));
    TEST_MSG("a slice this backend created must be given back");

    close_on_mock(rig, &m);
}


/* A slice the operator already had is not this backend's to remove. */
void test_split_off_leaves_a_slice_it_did_not_create(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    /* No split was turned on here, so nothing was borrowed. */
    TEST_CHECK(rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_OFF,
                                 RIG_VFO_CURR) == RIG_OK);
    TEST_CHECK(!smartsdr_mock_saw(&m, "slice remove"));

    close_on_mock(rig, &m);
}


/* ================================================================== */
/* Tuning the transmit slice                                           */
/* ================================================================== */

/* The split frequency belongs to whichever slice holds the transmitter, which
 * is the whole point of split; tuning this rig's own slice would move the
 * receiver instead. */
void test_split_freq_tunes_the_transmit_slice(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "slice create", "1");

    rig = open_on_mock(&m);
    rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, RIG_VFO_SUB);
    smartsdr_mock_forget(&m);

    TEST_CHECK(rig_set_split_freq(rig, RIG_VFO_CURR, 14200000.0) == RIG_OK);

    TEST_CHECK(smartsdr_mock_saw(&m, "slice tune 1 14.200000"));
    TEST_MSG("the transmit slice is the one that moves");
    TEST_CHECK(!smartsdr_mock_saw(&m, "slice tune 0"));
    TEST_MSG("the receive slice must stay where it is");

    close_on_mock(rig, &m);
}


/* Setting a mode makes the radio reset that slice's filters to the mode's
 * default, so a width supplied in the same call has to be sent afterwards or it
 * is thrown away. Nothing pinned the order, and both commands are sent either
 * way, so presence alone cannot tell a working sequence from a broken one. */
void test_split_mode_sends_the_width_after_the_mode(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "slice create", "1");

    rig = open_on_mock(&m);
    TEST_CHECK(rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON,
                                 RIG_VFO_SUB) == RIG_OK);
    smartsdr_mock_forget(&m);

    TEST_CHECK(rig_set_split_mode(rig, RIG_VFO_SUB, RIG_MODE_LSB,
                                  2400) == RIG_OK);

    TEST_CHECK(smartsdr_mock_saw(&m, "slice set 1 mode=LSB"));
    TEST_MSG("the transmit slice's mode must be set");
    TEST_CHECK(smartsdr_mock_saw(&m, "filt 1 0 2400"));
    TEST_MSG("the width must be sent");
    TEST_CHECK(smartsdr_mock_saw_before(&m, "mode=LSB", "filt 1"));
    TEST_MSG("the width must follow the mode; sent first the radio discards it");

    close_on_mock(rig, &m);
}


/* A width of NOCHANGE must not send a filter command at all -- sending one
 * would overwrite whatever the operator had set. */
void test_split_mode_without_a_width_leaves_the_filters_alone(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "slice create", "1");

    rig = open_on_mock(&m);
    TEST_CHECK(rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON,
                                 RIG_VFO_SUB) == RIG_OK);
    smartsdr_mock_forget(&m);

    TEST_CHECK(rig_set_split_mode(rig, RIG_VFO_SUB, RIG_MODE_USB,
                                  RIG_PASSBAND_NOCHANGE) == RIG_OK);

    TEST_CHECK(smartsdr_mock_saw(&m, "slice set 1 mode=USB"));
    TEST_CHECK(!smartsdr_mock_saw(&m, "filt 1"));
    TEST_MSG("no width was asked for, so none should have been sent");

    close_on_mock(rig, &m);
}


TEST_LIST =
{
    { "split_mode_sends_the_width_after_the_mode", test_split_mode_sends_the_width_after_the_mode },
    { "split_mode_without_a_width_leaves_the_filters_alone", test_split_mode_without_a_width_leaves_the_filters_alone },
    { "vfo_names_map_to_the_right_receiver", test_vfo_names_map_to_the_right_receiver },
    { "selecting_sub_without_a_second_slice_is_refused", test_selecting_sub_without_a_second_slice_is_refused },
    { "reading_the_vfo_reports_main_with_one_slice", test_reading_the_vfo_reports_main_with_one_slice },
    { "split_on_creates_a_slice_and_gives_it_the_transmitter", test_split_on_creates_a_slice_and_gives_it_the_transmitter },
    { "split_on_twice_creates_only_one_slice", test_split_on_twice_creates_only_one_slice },
    { "split_off_returns_the_transmitter_and_the_slice", test_split_off_returns_the_transmitter_and_the_slice },
    { "split_off_leaves_a_slice_it_did_not_create", test_split_off_leaves_a_slice_it_did_not_create },
    { "split_freq_tunes_the_transmit_slice", test_split_freq_tunes_the_transmit_slice },
    { NULL, NULL }
};
