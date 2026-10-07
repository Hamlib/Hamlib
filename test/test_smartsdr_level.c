/*
 *  Hamlib SmartSDR level and function mapping tests
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

/* Which command a Hamlib level, function or PTT call turns into on the wire.
 * The maps that decide this are private to the backend, so each case drives
 * the public API and checks what actually reached the radio. Several of these
 * were advertised for a long time while every write was rejected, because the
 * command they built was not one the radio accepts. */

#include "acutest.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>

#include "smartsdr_mock.h"

#define MOCK_TIMEOUT_MS   "300"
#define MOCK_LIVENESS_MS  "5000"


/* A rig pointed at the mock and opened, ready to take commands. */
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


/* Set one level and report the command the radio saw. */
static void check_level(setting_t level, value_t val, const char *expect)
{
    struct smartsdr_mock m;
    RIG *rig;
    int rc;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);

    /* Only what this level produces is of interest, not the open sequence. */
    smartsdr_mock_forget(&m);

    rc = rig_set_level(rig, RIG_VFO_CURR, level, val);
    TEST_CHECK(rc == RIG_OK);
    TEST_MSG("rig_set_level returned %s", rigerror(rc));

    TEST_CHECK(smartsdr_mock_saw(&m, expect));
    TEST_MSG("expected the radio to see \"%s\"", expect);

    close_on_mock(rig, &m);
}


static void check_func(setting_t func, int status, const char *expect)
{
    struct smartsdr_mock m;
    RIG *rig;
    int rc;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    rc = rig_set_func(rig, RIG_VFO_CURR, func, status);
    TEST_CHECK(rc == RIG_OK);
    TEST_MSG("rig_set_func returned %s", rigerror(rc));

    TEST_CHECK(smartsdr_mock_saw(&m, expect));
    TEST_MSG("expected the radio to see \"%s\"", expect);

    close_on_mock(rig, &m);
}


/* ================================================================== */
/* Levels on the slice object                                          */
/* ================================================================== */

/* A fraction becomes a percentage, addressed to this rig's own slice. */
void test_level_af_scales_to_percent(void)
{
    value_t v;

    v.f = 0.5f;
    check_level(RIG_LEVEL_AF, v, "slice set 0 audio_level=50");
}


void test_level_rf_scales_to_percent(void)
{
    value_t v;

    v.f = 0.25f;
    check_level(RIG_LEVEL_RF, v, "slice set 0 rfgain=25");
}


/* ================================================================== */
/* Levels on the transmit object                                       */
/* ================================================================== */

void test_level_rfpower_goes_to_transmit(void)
{
    value_t v;

    v.f = 0.01f;
    check_level(RIG_LEVEL_RFPOWER, v, "transmit set rfpower=1");
}


/* The radio reports this as mic_level but only accepts miclevel, so the
 * property carries a separate name for writing. */
void test_level_micgain_writes_miclevel(void)
{
    value_t v;

    v.f = 0.4f;
    check_level(RIG_LEVEL_MICGAIN, v, "transmit set miclevel=40");
}


void test_level_micgain_does_not_write_mic_level(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    value_t v;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    v.f = 0.4f;
    rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_MICGAIN, v);

    /* The reported name is rejected by the radio; writing it would look like
     * success here while changing nothing. */
    TEST_CHECK(!smartsdr_mock_saw(&m, "mic_level="));

    close_on_mock(rig, &m);
}


/* ================================================================== */
/* CW levels, which are positional commands of their own               */
/* ================================================================== */

void test_level_keyspd_builds_cw_wpm(void)
{
    value_t v;

    v.i = 25;
    check_level(RIG_LEVEL_KEYSPD, v, "cw wpm 25");
}


void test_level_cwpitch_builds_cw_pitch(void)
{
    value_t v;

    v.i = 700;
    check_level(RIG_LEVEL_CWPITCH, v, "cw pitch 700");
}


void test_level_bkin_dlyms_builds_cw_break_in_delay(void)
{
    value_t v;

    v.i = 50;
    check_level(RIG_LEVEL_BKIN_DLYMS, v, "cw break_in_delay 50");
}


/* These four are the ones that were advertised while every write failed,
 * because they were sent as "transmit set <key>=<value>" which the radio
 * rejects for them. */
void test_cw_levels_are_not_sent_as_transmit_set(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    value_t v;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    v.i = 25;
    rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_KEYSPD, v);
    v.i = 700;
    rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_CWPITCH, v);

    TEST_CHECK(!smartsdr_mock_saw(&m, "transmit set speed="));
    TEST_CHECK(!smartsdr_mock_saw(&m, "transmit set pitch="));

    close_on_mock(rig, &m);
}


/* ================================================================== */
/* Functions                                                           */
/* ================================================================== */

void test_func_mute_goes_to_the_slice(void)
{
    check_func(RIG_FUNC_MUTE, 1, "slice set 0 audio_mute=1");
}


void test_func_vox_goes_to_transmit(void)
{
    check_func(RIG_FUNC_VOX, 1, "transmit set vox_enable=1");
}


void test_func_fbkin_builds_cw_break_in(void)
{
    check_func(RIG_FUNC_FBKIN, 1, "cw break_in 1");
}


/* Reported as sb_monitor, accepted only as mon. */
void test_func_mon_writes_mon(void)
{
    check_func(RIG_FUNC_MON, 1, "transmit set mon=1");
}


void test_func_mon_does_not_write_sb_monitor(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_MON, 1);

    TEST_CHECK(!smartsdr_mock_saw(&m, "sb_monitor="));

    close_on_mock(rig, &m);
}


void test_func_off_writes_zero(void)
{
    check_func(RIG_FUNC_VOX, 0, "transmit set vox_enable=0");
}


/* ================================================================== */
/* PTT                                                                 */
/* ================================================================== */

/* Keying a slice that does not hold the transmitter would put a different
 * slice on the air, so the guard reads this slice's own tx= and refuses.
 * The status is edited before the rig opens, so the backend never sees a
 * slice claiming the transmitter. */
void test_ptt_refused_when_the_slice_has_no_transmitter(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    char *p;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    /* The leading space keeps this off txant=. */
    p = strstr(m.slice_status, " tx=1");
    TEST_ASSERT(p != NULL);
    p[4] = '0';

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    TEST_CHECK(rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_ON) == -RIG_ENTARGET);
    TEST_CHECK(!smartsdr_mock_saw(&m, "xmit 1"));
    TEST_MSG("a slice without the transmitter must not key the radio");

    close_on_mock(rig, &m);
}

/* Keying routes this slice's DAX audio to the transmitter. The radio rejects
 * the command unless slice= accompanies tx=, so both operands must be there;
 * sent without slice= it failed on every key and the routing never happened. */
void test_ptt_on_binds_dax_with_slice(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_ON);

    TEST_CHECK(smartsdr_mock_saw(&m, "dax audio set 1 slice=0 tx=1"));
    TEST_MSG("keying must bind DAX channel 1 to slice 0");

    close_on_mock(rig, &m);
}


/* The bare spelling is what the radio answers 0x50001000 to. */
void test_ptt_on_does_not_send_dax_without_slice(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_ON);

    TEST_CHECK(!smartsdr_mock_saw(&m, "dax audio set 1 tx=1"));

    close_on_mock(rig, &m);
}


void test_ptt_on_keys_the_transmitter(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_ON);

    TEST_CHECK(smartsdr_mock_saw(&m, "xmit 1"));

    close_on_mock(rig, &m);
}


/* Unkeying claims no DAX routing; it only has to stop the transmitter. */
void test_ptt_off_unkeys_without_dax(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);

    TEST_CHECK(smartsdr_mock_saw(&m, "xmit 0"));
    TEST_CHECK(!smartsdr_mock_saw(&m, "dax audio set"));

    close_on_mock(rig, &m);
}


/* An unkey has to reach the radio even when the command before it is refused,
 * or a rejected transmitter claim would leave the radio on the air. */
void test_ptt_off_unkeys_even_when_the_slice_claim_fails(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = open_on_mock(&m);
    smartsdr_mock_forget(&m);

    smartsdr_mock_fail(&m, "slice set 0 tx=1", "50001000");
    rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);

    TEST_CHECK(smartsdr_mock_saw(&m, "xmit 0"));
    TEST_MSG("the radio must still be told to stop transmitting");

    close_on_mock(rig, &m);
}


/* A level with no range is a level an application has to guess at: it cannot
 * size a control or reject a bad value. Every level this backend advertises
 * carries one. */
void test_every_advertised_level_has_a_range(void)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR);
    setting_t advertised;
    int i;

    TEST_ASSERT(rig != NULL);
    advertised = rig->caps->has_get_level | rig->caps->has_set_level;

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        setting_t level = rig_idx2setting(i);
        const gran_t *g;

        if (level == 0 || !(advertised & level))
        {
            continue;
        }

        g = &rig->caps->level_gran[i];

        /* Float and integer levels use different members of the union, so a
         * range is present when either side of it is set. */
        TEST_CHECK(g->max.f != 0.0f || g->max.i != 0);
        TEST_MSG("%s has no range", rig_strlevel(level));
    }

    rig_cleanup(rig);
}


/* The antenna tuner is not the tune carrier. Mapping the two put the radio on
 * the air when an application asked for the tuner, so it is not advertised. */
void test_tuner_is_not_advertised(void)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR);

    TEST_ASSERT(rig != NULL);
    TEST_CHECK((rig->caps->has_set_func & RIG_FUNC_TUNER) == 0);
    TEST_CHECK((rig->caps->has_get_func & RIG_FUNC_TUNER) == 0);
    rig_cleanup(rig);
}


TEST_LIST =
{
    { "level_af_scales_to_percent",             test_level_af_scales_to_percent },
    { "level_rf_scales_to_percent",             test_level_rf_scales_to_percent },
    { "level_rfpower_goes_to_transmit",         test_level_rfpower_goes_to_transmit },
    { "level_micgain_writes_miclevel",          test_level_micgain_writes_miclevel },
    { "level_micgain_does_not_write_mic_level", test_level_micgain_does_not_write_mic_level },
    { "level_keyspd_builds_cw_wpm",             test_level_keyspd_builds_cw_wpm },
    { "level_cwpitch_builds_cw_pitch",          test_level_cwpitch_builds_cw_pitch },
    { "level_bkin_dlyms_builds_cw_break_in_delay", test_level_bkin_dlyms_builds_cw_break_in_delay },
    { "cw_levels_are_not_sent_as_transmit_set", test_cw_levels_are_not_sent_as_transmit_set },
    { "func_mute_goes_to_the_slice",            test_func_mute_goes_to_the_slice },
    { "func_vox_goes_to_transmit",              test_func_vox_goes_to_transmit },
    { "func_fbkin_builds_cw_break_in",          test_func_fbkin_builds_cw_break_in },
    { "func_mon_writes_mon",                    test_func_mon_writes_mon },
    { "func_mon_does_not_write_sb_monitor",     test_func_mon_does_not_write_sb_monitor },
    { "func_off_writes_zero",                   test_func_off_writes_zero },
    { "ptt_refused_when_the_slice_has_no_transmitter", test_ptt_refused_when_the_slice_has_no_transmitter },
    { "ptt_on_binds_dax_with_slice",            test_ptt_on_binds_dax_with_slice },
    { "ptt_on_does_not_send_dax_without_slice", test_ptt_on_does_not_send_dax_without_slice },
    { "ptt_on_keys_the_transmitter",            test_ptt_on_keys_the_transmitter },
    { "ptt_off_unkeys_without_dax",             test_ptt_off_unkeys_without_dax },
    { "ptt_off_unkeys_even_when_the_slice_claim_fails", test_ptt_off_unkeys_even_when_the_slice_claim_fails },
    { "every_advertised_level_has_a_range",     test_every_advertised_level_has_a_range },
    { "tuner_is_not_advertised",                test_tuner_is_not_advertised },
    { NULL, NULL }
};
