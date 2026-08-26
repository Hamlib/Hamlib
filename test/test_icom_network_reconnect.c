/*
 *  Hamlib Icom network reconnect tests
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

/* How the backend notices a session it has lost, and what it does next.
 *
 * A radio can go away in two distinguishable ways, and the difference matters
 * to an application: it can fall off the network, which only silence reveals,
 * or it can hand the session to another client and say so. The mock radio can
 * do both on demand, which is what makes this testable without hardware --
 * against a real radio the first case means physically removing it. */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <hamlib/rig.h>
#include "hamlib/riglist.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "icom.h"
#include "icom_defs.h"
#include "icom_network_mock.h"

/* The token floor is 1000 ms, so a lost session cannot be detected faster than
 * that; the waits below allow for it plus the backend's own poll interval. */
#define TEST_LIVENESS_MS   1000
#define TEST_LOSS_WAIT_MS  6000
#define TEST_POLL_MS       50


static void sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}


static RIG *open_against_mock(struct mock_server *m, const char *extra_conf)
{
    char port[16];
    RIG *rig = rig_init(RIG_MODEL_IC7610NET);

    if (rig == NULL) { return NULL; }

    SNPRINTF(port, sizeof(port), "%u", (unsigned)m->ctrl_port);
    rig_set_conf(rig, TOK_NET_USERNAME, "user");
    rig_set_conf(rig, TOK_NET_PASSWORD, "pass");
    rig_set_conf(rig, TOK_NET_CONTROL_PORT, port);

    if (extra_conf != NULL)
    {
        char copy[128];
        char *tok, *saveptr = NULL;
        SNPRINTF(copy, sizeof(copy), "%s", extra_conf);

        for (tok = strtok_r(copy, ",", &saveptr); tok != NULL;
                tok = strtok_r(NULL, ",", &saveptr))
        {
            char *eq = strchr(tok, '=');

            if (eq == NULL) { continue; }

            *eq = '\0';
            rig_set_conf(rig, rig_token_lookup(rig, tok), eq + 1);
        }
    }

    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), "127.0.0.1");

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        return NULL;
    }

    return rig;
}


/* Wait for the backend to publish a loss, returning the reason it gave. */
static int wait_for_loss(RIG *rig, int timeout_ms)
{
    int waited = 0;

    while (waited < timeout_ms)
    {
        if (STATE(rig)->comm_status == RIG_COMM_STATUS_DISCONNECTED)
        {
            return (int)STATE(rig)->comm_reason;
        }

        sleep_ms(TEST_POLL_MS);
        waited += TEST_POLL_MS;
    }

    return -1;
}


/* A radio that stops answering is only detectable by its silence, so the
 * liveness timeout is the only thing that can report it. */
void test_silence_is_reported_as_link_timeout(void)
{
    struct mock_server mock;
    RIG *rig;
    int reason;

    mock_start(&mock);
    rig = open_against_mock(&mock, "net_liveness_timeout=1000");
    TEST_ASSERT(rig != NULL);
    TEST_CHECK(STATE(rig)->comm_status == RIG_COMM_STATUS_OK);

    mock.go_silent = 1;
    reason = wait_for_loss(rig, TEST_LOSS_WAIT_MS);

    TEST_CHECK(reason == RIG_COMM_REASON_LINK_TIMEOUT);
    TEST_MSG("silence must be reported as a link timeout, got reason %d",
             reason);

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* A radio that hands the session to another client says so, and that is a
 * different answer from silence: reconnecting immediately would only take the
 * session back from whoever now holds it. */
void test_announced_disconnect_is_reported_as_peer(void)
{
    struct mock_server mock;
    RIG *rig;
    int reason;

    mock_start(&mock);
    rig = open_against_mock(&mock, "net_liveness_timeout=0");
    TEST_ASSERT(rig != NULL);

    mock.announce_disconnect = 1;
    reason = wait_for_loss(rig, TEST_LOSS_WAIT_MS);

    TEST_CHECK(reason == RIG_COMM_REASON_PEER_DISCONNECT);
    TEST_MSG("an announced disconnect must not read as a timeout, got %d",
             reason);

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* With auto-reconnect off -- the default -- a lost session stays lost. The
 * backend must not quietly reconnect behind an application that never asked
 * for it and is still holding the old state. */
void test_no_reconnect_unless_asked(void)
{
    struct mock_server mock;
    RIG *rig;

    mock_start(&mock);
    rig = open_against_mock(&mock, "net_liveness_timeout=1000");
    TEST_ASSERT(rig != NULL);

    mock.go_silent = 1;
    TEST_ASSERT(wait_for_loss(rig, TEST_LOSS_WAIT_MS) >= 0);

    /* The radio comes back, but nobody asked us to chase it. */
    mock.go_silent = 0;
    sleep_ms(2000);

    TEST_CHECK(STATE(rig)->comm_status == RIG_COMM_STATUS_DISCONNECTED);
    TEST_MSG("auto-reconnect is opt-in and was not requested");

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* With auto-reconnect on, a radio that comes back is picked up again without
 * the application doing anything. */
void test_auto_reconnect_recovers_the_session(void)
{
    struct mock_server mock;
    RIG *rig;
    int waited = 0;

    mock_start(&mock);
    rig = open_against_mock(&mock,
                            "net_liveness_timeout=1000,net_auto_reconnect=1");
    TEST_ASSERT(rig != NULL);

    mock.go_silent = 1;
    TEST_ASSERT(wait_for_loss(rig, TEST_LOSS_WAIT_MS) >= 0);

    mock.go_silent = 0;

    /* The reconnect thread backs off between attempts, so allow several. */
    while (waited < 20000
            && STATE(rig)->comm_status != RIG_COMM_STATUS_OK)
    {
        sleep_ms(200);
        waited += 200;
    }

    TEST_CHECK(STATE(rig)->comm_status == RIG_COMM_STATUS_OK);
    TEST_MSG("the session did not come back after %d ms", waited);

    rig_close(rig);
    rig_cleanup(rig);
    mock_stop(&mock);
}


/* Losing and re-establishing repeatedly must not accumulate anything. The
 * failure this guards against is a session freed on one path and not another,
 * which shows up as a wedge or a leak only after several cycles. */
void test_repeated_loss_cycles_are_clean(void)
{
    int cycle;

    for (cycle = 0; cycle < 3; cycle++)
    {
        struct mock_server mock;
        RIG *rig;

        mock_start(&mock);
        rig = open_against_mock(&mock, "net_liveness_timeout=1000");
        TEST_ASSERT(rig != NULL);

        mock.go_silent = 1;
        TEST_CHECK(wait_for_loss(rig, TEST_LOSS_WAIT_MS) >= 0);
        TEST_MSG("cycle %d never reported the loss", cycle);

        /* Closing a rig whose session is already gone must still work. */
        rig_close(rig);
        rig_cleanup(rig);
        mock_stop(&mock);
    }
}


TEST_LIST =
{
    { "silence_is_reported_as_link_timeout", test_silence_is_reported_as_link_timeout },
    { "announced_disconnect_is_reported_as_peer", test_announced_disconnect_is_reported_as_peer },
    { "no_reconnect_unless_asked",      test_no_reconnect_unless_asked },
    { "auto_reconnect_recovers_the_session", test_auto_reconnect_recovers_the_session },
    { "repeated_loss_cycles_are_clean", test_repeated_loss_cycles_are_clean },
    { NULL, NULL }
};
