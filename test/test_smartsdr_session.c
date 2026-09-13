/*
 *  Hamlib SmartSDR control session tests
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

/* Tests for the SmartSDR control session, driven by the in-process mock radio */
/* so that the situations a working radio never produces -- a radio that goes  */
/* quiet, one that refuses the next connection, one that answers a command     */
/* with an empty body -- are things a test can arrange.                        */

#include "acutest.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <hamlib/rig_state.h>

#include "smartsdr_mock.h"
#include "smartsdr_priv.h"
#include "smartsdr_props.h"
#include "smartsdr_session.h"
#include "smartsdr_slice.h"


/* A radio on the far side of loopback answers at once, so the wait for a
 * reply can be short. The default would make every deliberate silence in
 * this file take seconds to notice. */
#define MOCK_TIMEOUT_MS "200"

/* How long a mock radio may say nothing before the session is declared lost.
 * The shipped default is twenty seconds, which every test that arranges a
 * silence would otherwise have to sit through. */
#define MOCK_LIVENESS_MS "2000"


static struct rig_state *state_of(RIG *rig)
{
    return (struct rig_state *)rig_data_pointer(rig, RIG_PTRX_STATE);
}


static struct smartsdr_priv_data *priv_of(RIG *rig)
{
    return (struct smartsdr_priv_data *)state_of(rig)->priv;
}


/* A rig pointed at the mock, configured but not yet opened. */
static RIG *rig_on_mock(struct smartsdr_mock *m, rig_model_t model)
{
    RIG *rig = rig_init(model);
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

    return rig;
}


/* A refused command in the open sequence has to abort the open. Handing back
 * a rig whose subscriptions never took would leave it accepting commands and
 * reporting stale state, which is worse than failing to open. */
void test_open_fails_when_a_step_is_refused(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);
    smartsdr_mock_fail(&m, "sub slice", "50000001");

    TEST_CHECK(rig_open(rig) != RIG_OK);
    TEST_MSG("the open must not succeed when a step was refused");

    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* Present the radio as having slice B and no slice A. The backend learns which
 * slices exist from status, not from a listing, so this is what "the slice is
 * not there" looks like on the wire. */
static void only_slice_b(struct smartsdr_mock *m)
{
    char *p;

    strncpy(m->slice_status, smartsdr_mock_default_slice,
            sizeof(m->slice_status) - 1);
    m->slice_status[sizeof(m->slice_status) - 1] = '\0';

    p = strstr(m->slice_status, "slice 0 ");

    if (p)
    {
        p[6] = '1';
    }

    p = strstr(m->slice_status, "index_letter=A");

    if (p)
    {
        p[13] = 'B';
    }
}


/* The configured slice is absent and the radio hands out a different index
 * than the one asked for. Which index it picks is the radio's decision and
 * cannot be influenced, so this is driven from the mock: the FLEX-8400M
 * always reuses the lowest free index, which means the hardware can never
 * produce the mismatch these two cases turn on. */
void test_slice_adopted_when_none_was_named(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);
    /* Declare slice B instead of A, so the slice this rig defaults to is not
     * on the radio and has to be created. */
    only_slice_b(&m);
    smartsdr_mock_reply(&m, "slice create", "3");

    TEST_CHECK(rig_open(rig) == RIG_OK);
    TEST_CHECK(priv_of(rig)->slicenum == 3);
    TEST_MSG("no slice was named, so the one the radio gave must be adopted");

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* Naming a slice and silently getting a different one would control the wrong
 * receiver, so the slice is handed back and the open fails instead. */
void test_named_slice_is_not_swapped_for_another(void)
{
    struct smartsdr_mock m;
    RIG *rig;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);
    TEST_ASSERT(rig_set_conf(rig, rig_token_lookup(rig, "slice"), "C")
                == RIG_OK);
    only_slice_b(&m);
    smartsdr_mock_reply(&m, "slice create", "3");

    TEST_CHECK(rig_open(rig) != RIG_OK);
    TEST_CHECK(smartsdr_mock_saw(&m, "slice remove 3"));
    TEST_MSG("the unwanted slice must be given back, not left behind");

    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* Read whatever the radio has pushed. Status arrives unsolicited, so a
 * command is sent to give the reader something to wait on. */
static void pump(RIG *rig)
{
    char ping[] = "ping";

    usleep(50 * 1000);
    smartsdr_transaction_resp(rig, ping, NULL, 0);
}


/* ================================================================== */
/* Status routing                                                      */
/* ================================================================== */

/* Slices belong to the radio, not to a client, so status for every one of
 * them arrives on this rig's connection. Only the rig's own slice may change
 * what it reports. Confirmed against hardware: with slice A on 14.100 USB and
 * slice B on 7.100 LSB, the rig bound to A reported B's values. */
void test_status_for_another_slice_does_not_leak(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    struct smartsdr_priv_data *priv;
    freq_t freq_before;
    rmode_t mode_before;
    int ptt_before;
    int audio_before;
    int waited;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    priv = priv_of(rig);
    freq_before = smartsdr_slice_freq(priv);
    mode_before = smartsdr_slice_mode(priv);
    ptt_before = smartsdr_slice_ptt(priv);
    audio_before = priv->props[SMARTSDR_P_AUDIO_LEVEL].ival;

    TEST_CHECK(freq_before == (freq_t)14100000);
    TEST_MSG("the rig did not pick up its own slice: freq=%.0f",
             (double)freq_before);

    /* Another slice reports a different frequency, mode, transmit state and
     * audio level. */
    smartsdr_mock_push_status(&m,
                              "slice 1 in_use=1 RF_frequency=7.100000 "
                              "index_letter=B mode=LSB filter_lo=100 "
                              "filter_hi=3000 tx=1 active=1 audio_level=99 "
                              "state=TRANSMITTING");

    for (waited = 0; waited < 2000 && !priv->slices[1].in_use; waited += 50)
    {
        pump(rig);
    }

    /* The radio-wide slice table does record it, which is how split is
     * answered; what must not move is this rig's own state. */
    TEST_CHECK(priv->slices[1].in_use);
    TEST_MSG("the other slice never arrived over the connection");

    TEST_CHECK(smartsdr_slice_freq(priv) == freq_before);
    TEST_MSG("frequency leaked: %.0f, expected %.0f",
             (double)smartsdr_slice_freq(priv), (double)freq_before);

    TEST_CHECK(smartsdr_slice_mode(priv) == mode_before);
    TEST_MSG("mode leaked: %s, expected %s",
             rig_strrmode(smartsdr_slice_mode(priv)),
             rig_strrmode(mode_before));

    TEST_CHECK(smartsdr_slice_ptt(priv) == ptt_before);
    TEST_MSG("ptt leaked: %d, expected %d", smartsdr_slice_ptt(priv),
             ptt_before);

    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].ival == audio_before);
    TEST_MSG("audio_level leaked: %d, expected %d",
             priv->props[SMARTSDR_P_AUDIO_LEVEL].ival, audio_before);

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* ================================================================== */
/* Meter definitions                                                   */
/* ================================================================== */

/* The radio names its meters in status lines and gives each an index, and it
 * describes several meters in one line. A reader that stopped at the first
 * one left every later meter unnamed, and an unnamed meter can never be
 * matched to the values streaming in, so those levels simply never worked. */
void test_meter_definitions_several_on_one_line(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    struct smartsdr_priv_data *priv;
    int waited;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    priv = priv_of(rig);

    smartsdr_mock_push_status(&m,
                              "meter "
                              "16.src=SLC#16.num=0#16.nam=LEVEL#16.low=-150.0"
                              "#16.hi=20.0#16.desc=Signal level#16.unit=dBm"
                              "#16.fps=10"
                              "#17.src=COD#17.num=0#17.nam=SWR#17.low=1.0"
                              "#17.hi=10.0#17.unit=SWR#17.fps=10"
                              "#18.src=COD#18.num=1#18.nam=PATEMP"
                              "#18.unit=degC#18.fps=2"
                              "#19.src=COD#19.num=2#19.nam=+13.8B"
                              "#19.unit=Volts#19.fps=2"
                              "#20.src=COD#20.num=3#20.nam=PACURRENT"
                              "#20.unit=Amps#20.fps=2"
                              "#21.src=TX-#21.num=0#21.nam=ALC"
                              "#21.unit=dBFS#21.fps=10"
                              "#22.src=TX-#22.num=1#22.nam=FWDPWR"
                              "#22.unit=dBm#22.fps=10");

    for (waited = 0;
            waited < 2000 && priv->meter_wire_id[SMARTSDR_MTR_FWDPWR] < 0;
            waited += 50)
    {
        pump(rig);
    }

    /* The first meter on the line. */
    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_LEVEL] == 16);
    TEST_MSG("LEVEL is meter %d, expected 16",
             priv->meter_wire_id[SMARTSDR_MTR_LEVEL]);

    /* Every later one, which is what a reader that stopped early lost. */
    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_SWR] == 17);
    TEST_MSG("SWR is meter %d, expected 17",
             priv->meter_wire_id[SMARTSDR_MTR_SWR]);

    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_PATEMP] == 18);
    TEST_MSG("PATEMP is meter %d, expected 18",
             priv->meter_wire_id[SMARTSDR_MTR_PATEMP]);

    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_VOLTS] == 19);
    TEST_MSG("+13.8B is meter %d, expected 19",
             priv->meter_wire_id[SMARTSDR_MTR_VOLTS]);

    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_AMPS] == 20);
    TEST_MSG("PACURRENT is meter %d, expected 20",
             priv->meter_wire_id[SMARTSDR_MTR_AMPS]);

    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_ALC] == 21);
    TEST_MSG("ALC is meter %d, expected 21",
             priv->meter_wire_id[SMARTSDR_MTR_ALC]);

    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_FWDPWR] == 22);
    TEST_MSG("FWDPWR is meter %d, expected 22",
             priv->meter_wire_id[SMARTSDR_MTR_FWDPWR]);

    /* Each meter's rate belongs to that meter, not to the first one on the
     * line. */
    TEST_CHECK(priv->meter_fps[SMARTSDR_MTR_LEVEL] == 10);
    TEST_MSG("LEVEL rate is %d, expected 10",
             priv->meter_fps[SMARTSDR_MTR_LEVEL]);
    TEST_CHECK(priv->meter_fps[SMARTSDR_MTR_PATEMP] == 2);
    TEST_MSG("PATEMP rate is %d, expected 2",
             priv->meter_fps[SMARTSDR_MTR_PATEMP]);
    TEST_CHECK(priv->meter_fps[SMARTSDR_MTR_AMPS] == 2);
    TEST_MSG("PACURRENT rate is %d, expected 2",
             priv->meter_fps[SMARTSDR_MTR_AMPS]);

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* A meter with no rate is sent when it changes rather than on a timer. That
 * is a working meter, so it must still be named and still be readable; only
 * the rate is zero. */
void test_meter_definition_with_zero_fps(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    struct smartsdr_priv_data *priv;
    int waited;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    priv = priv_of(rig);

    smartsdr_mock_push_status(&m,
                              "meter "
                              "30.src=COD#30.num=0#30.nam=SWR#30.unit=SWR"
                              "#30.fps=0"
                              "#31.src=TX-#31.num=0#31.nam=ALC#31.unit=dBFS");

    for (waited = 0;
            waited < 2000 && priv->meter_wire_id[SMARTSDR_MTR_ALC] < 0;
            waited += 50)
    {
        pump(rig);
    }

    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_SWR] == 30);
    TEST_MSG("SWR is meter %d, expected 30",
             priv->meter_wire_id[SMARTSDR_MTR_SWR]);
    TEST_CHECK(priv->meter_fps[SMARTSDR_MTR_SWR] == 0);
    TEST_MSG("SWR rate is %d, expected 0", priv->meter_fps[SMARTSDR_MTR_SWR]);

    /* A meter that names no rate at all is the same case. */
    TEST_CHECK(priv->meter_wire_id[SMARTSDR_MTR_ALC] == 31);
    TEST_MSG("ALC is meter %d, expected 31",
             priv->meter_wire_id[SMARTSDR_MTR_ALC]);
    TEST_CHECK(priv->meter_fps[SMARTSDR_MTR_ALC] == 0);
    TEST_MSG("ALC rate is %d, expected 0", priv->meter_fps[SMARTSDR_MTR_ALC]);

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* ================================================================== */
/* Losing the radio                                                    */
/* ================================================================== */

/* The radio answers every command, so a keepalive that goes unanswered is the
 * link being gone. Nothing more will arrive after that, so an application
 * reading a stream has to be told rather than left waiting for samples that
 * are never coming. */
void test_keepalive_timeout_disconnects_and_fails_open_streams(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    struct rig_state *rs;
    struct rig_stream_config cfg;
    rig_stream_t *stream = NULL;
    float buf[2048];
    size_t nread = 0;
    int ret;
    int waited;
    int attempt;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "stream create", "0x04000001");

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_F32;
    cfg.sample_rate = 24000;
    cfg.channels = 2;

    if (rig_stream_open(rig, &cfg, &stream) != RIG_OK)
    {
        rig_close(rig);
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_stream_open AUDIO_RX against the mock failed");
    }

    rs = state_of(rig);
    TEST_CHECK(rs->comm_status == RIG_COMM_STATUS_OK);
    TEST_MSG("the session was not healthy before the radio went quiet");

    /* The radio holds the connection open and stops answering, which is what
     * it looks like when it loses power or falls off the network. */
    m.go_silent = 1;

    /* Silence for longer than the configured budget is the loss, so allow
     * well over one budget for it to be noticed. */
    for (waited = 0;
            waited < 15000 && rs->comm_status != RIG_COMM_STATUS_DISCONNECTED;
            waited += 250)
    {
        usleep(250 * 1000);
    }

    TEST_CHECK(rs->comm_status == RIG_COMM_STATUS_DISCONNECTED);
    TEST_MSG("comm_status is %d after %d ms of silence, expected DISCONNECTED",
             (int)rs->comm_status, waited);

    TEST_CHECK(rs->comm_reason == RIG_COMM_REASON_LINK_TIMEOUT);
    TEST_MSG("comm_reason is %s, expected LINK_TIMEOUT",
             rig_strcommreason(rs->comm_reason));

    /* Samples buffered before the loss are still good and are delivered
     * first, and the dispatcher needs a moment to notice, so poll. */
    ret = RIG_OK;

    for (attempt = 0; attempt < 20 && ret != -RIG_EIO; attempt++)
    {
        nread = 0;
        ret = rig_stream_read(rig, stream, buf, sizeof(buf), &nread, 500,
                              NULL);
    }

    TEST_CHECK(ret == -RIG_EIO);
    TEST_MSG("stream read returned %d (%s), expected -RIG_EIO", ret,
             rigerror(ret));

    /* Let the radio answer again so closing does not wait out a timeout for
     * every command it sends. */
    m.go_silent = 0;

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* A rebuilt session needs its own keepalive. The old one exits when it finds
 * the radio gone, and the radio drops a client that stops pinging, so a
 * session that came back without one dies again about fifteen seconds later
 * and keeps doing so. */
void test_reconnect_reestablishes_and_restarts_keepalive(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    struct rig_state *rs;
    struct smartsdr_priv_data *priv;
    int connections_before;
    int waited;
    int pings;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);
    rig_set_conf(rig, rig_token_lookup(rig, "auto_reconnect"), "1");

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    rs = state_of(rig);
    priv = priv_of(rig);

    TEST_CHECK(priv->reconnect_running);
    TEST_MSG("auto_reconnect=1 did not start the reconnect thread");

    connections_before = m.connections;

    /* The radio is up but will not take a session, so the first attempt to
     * rebuild it fails and the backoff applies. */
    m.refuse = 1;

    /* How the loss is discovered is the keepalive's job and is tested on its
     * own; what is under test here is what happens once it is known. */
    smartsdr_session_lost(rig, RIG_COMM_REASON_LINK_TIMEOUT);

    for (waited = 0; waited < 5000 && m.connections == connections_before;
            waited += 100)
    {
        usleep(100 * 1000);
    }

    TEST_CHECK(m.connections > connections_before);
    TEST_MSG("no reconnect was attempted in %d ms", waited);

    TEST_CHECK(rs->comm_status == RIG_COMM_STATUS_DISCONNECTED);
    TEST_MSG("a refused reconnect was reported as a healthy session");

    /* A failed attempt must not rewrite why the radio was lost. */
    TEST_CHECK(rs->comm_reason == RIG_COMM_REASON_LINK_TIMEOUT);
    TEST_MSG("the reason became %s", rig_strcommreason(rs->comm_reason));

    /* The radio comes back. */
    m.refuse = 0;

    for (waited = 0; waited < 30000 && rs->comm_status != RIG_COMM_STATUS_OK;
            waited += 250)
    {
        usleep(250 * 1000);
    }

    TEST_CHECK(rs->comm_status == RIG_COMM_STATUS_OK);
    TEST_MSG("the session was not re-established in %d ms", waited);
    TEST_CHECK(rs->comm_reason == RIG_COMM_REASON_NONE);
    TEST_MSG("a re-established session still reports %s",
             rig_strcommreason(rs->comm_reason));
    TEST_CHECK(!priv->session_lost);

    /* The rebuilt session identified itself again, or the radio grants it
     * nothing. */
    TEST_CHECK(smartsdr_mock_count(&m, "client gui") >= 2);
    TEST_MSG("the rebuilt session did not register as a client");

    if (rs->comm_status != RIG_COMM_STATUS_OK)
    {
        rig_close(rig);
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        return;
    }

    /* Nothing but the keepalive pings once the rig is open, so a ping
     * arriving now is the replacement keepalive doing its job. */
    smartsdr_mock_forget(&m);
    pings = smartsdr_mock_wait_for(&m, "ping", 1, 10000);

    TEST_CHECK(pings >= 1);
    TEST_MSG("the re-established session sent no keepalive in 10 s");

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* ================================================================== */
/* Creating a slice                                                    */
/* ================================================================== */

/* "slice create" reports success with an empty body when it is given no
 * frequency, and the radio picks the index itself. An empty body read as
 * slice 0 names the operator's own slice, which the backend would then remove
 * as if it had created it. */
void test_slice_create_empty_body_is_not_slice_zero(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    int rc;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    /* Success, and nothing where the new slice number belongs. */
    smartsdr_mock_reply(&m, "slice create", "");

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    /* Slice B, which the mock's radio does not have, so opening the rig has
     * to create one. */
    rig_set_conf(rig, rig_token_lookup(rig, "slice"), "B");
    rig_set_conf(rig, rig_token_lookup(rig, "slice_missing"), "create");

    rc = rig_open(rig);

    TEST_CHECK(rc != RIG_OK);
    TEST_MSG("opening the rig succeeded on a slice that was never created");

    TEST_CHECK(smartsdr_mock_saw(&m, "slice create"));
    TEST_MSG("the backend never asked for a slice");

    TEST_CHECK(!smartsdr_mock_saw(&m, "slice remove"));
    TEST_MSG("an empty slice number was read as slice 0 and slice 0 removed");

    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* When the radio does name the slice it created, that is the slice the rig
 * takes, and the rig gives it back when it closes. */
void test_slice_create_body_names_the_created_slice(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    struct smartsdr_priv_data *priv;
    int rc;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    /* Slice D is asked for and slice D is what the radio made. */
    smartsdr_mock_reply(&m, "slice create", "3");

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);
    rig_set_conf(rig, rig_token_lookup(rig, "slice"), "D");
    rig_set_conf(rig, rig_token_lookup(rig, "slice_missing"), "create");

    rc = rig_open(rig);
    TEST_CHECK(rc == RIG_OK);
    TEST_MSG("rig_open returned %d (%s)", rc, rigerror(rc));

    if (rc != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        return;
    }

    priv = priv_of(rig);

    TEST_CHECK(priv->opened_slice == 3);
    TEST_MSG("the rig took slice %d, expected 3", priv->opened_slice);
    TEST_CHECK(priv->slices[3].in_use);
    TEST_MSG("the created slice was not marked in use");

    /* A slice created because the configured one was absent goes with us. */
    rig_close(rig);

    TEST_CHECK(smartsdr_mock_saw(&m, "slice remove 3"));
    TEST_MSG("closing the rig left the slice it created behind");

    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* A rig that names a slice the radio does not have, and is told to fail
 * rather than create one, must not create one. */
void test_slice_missing_fail_does_not_create(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    int rc;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "slice create", "1");

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);
    rig_set_conf(rig, rig_token_lookup(rig, "slice"), "B");
    rig_set_conf(rig, rig_token_lookup(rig, "slice_missing"), "fail");

    rc = rig_open(rig);

    TEST_CHECK(rc != RIG_OK);
    TEST_MSG("opening the rig succeeded with slice_missing=fail");

    TEST_CHECK(!smartsdr_mock_saw(&m, "slice create"));
    TEST_MSG("a slice was created despite slice_missing=fail");

    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* ================================================================== */
/* Test list                                                           */
/* ================================================================== */


/* ================================================================== */
/* The control reader                                                  */
/* ================================================================== */

/* The reason the reader exists. Status arrives whenever the radio feels like
 * sending it, and a rig that is only streaming sends no commands at all, so
 * nothing would read it. Confirmed against hardware: before the reader, a
 * slice retuned by another client stayed stale until the next keepalive ping,
 * up to ten seconds later. */
void test_status_is_absorbed_with_no_command_in_flight(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    freq_t before = 0;
    freq_t after = 0;
    int waited;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    rig_get_freq(rig, RIG_VFO_A, &before);

    smartsdr_mock_push_status(&m,
                              "slice 0 in_use=1 RF_frequency=14.123000 "
                              "client_handle=0x" SMARTSDR_MOCK_HANDLE
                              " index_letter=A mode=USB");

    /* Deliberately no command is sent: only the reader can pick this up. */
    for (waited = 0; waited < 2000; waited += 50)
    {
        usleep(50 * 1000);
        rig_get_freq(rig, RIG_VFO_A, &after);

        if (after != before)
        {
            break;
        }
    }

    TEST_CHECK(after == 14123000);
    TEST_MSG("frequency was %.0f, expected 14123000 (was %.0f before)",
             (double)after, (double)before);

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


struct concurrent_caller
{
    RIG *rig;
    const char *cmd;
    char resp[256];
    int rc;
};


static void *concurrent_send(void *arg)
{
    struct concurrent_caller *c = arg;
    char cmd[64];

    strncpy(cmd, c->cmd, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';
    c->rc = smartsdr_transaction_resp(c->rig, cmd, c->resp, sizeof(c->resp));
    return NULL;
}


/* Replies are matched by the sequence number the caller used, so two commands
 * in flight at once must each get their own answer. Matching on anything less
 * specific would let one caller take the other's reply. */
void test_concurrent_callers_get_their_own_replies(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    struct concurrent_caller a;
    struct concurrent_caller b;
    pthread_t ta;
    pthread_t tb;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    smartsdr_mock_reply(&m, "meter list", "METERALPHA");
    smartsdr_mock_reply(&m, "sub tx", "TXBRAVO");

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.rig = rig;
    a.cmd = "meter list";
    b.rig = rig;
    b.cmd = "sub tx all";

    pthread_create(&ta, NULL, concurrent_send, &a);
    pthread_create(&tb, NULL, concurrent_send, &b);
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    TEST_CHECK(a.rc == RIG_OK);
    TEST_CHECK(b.rc == RIG_OK);
    TEST_CHECK(strstr(a.resp, "METERALPHA") != NULL);
    TEST_MSG("first caller got \"%s\"", a.resp);
    TEST_CHECK(strstr(b.resp, "TXBRAVO") != NULL);
    TEST_MSG("second caller got \"%s\"", b.resp);

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}



/* A command the radio never answers must still give the caller back its
 * thread. Before the reader existed this was the same code path as a healthy
 * command; now the waiter has to time out on its own. */
void test_a_command_with_no_reply_times_out(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    char cmd[] = "meter list";
    char resp[128];
    int rc;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    smartsdr_mock_swallow(&m, "meter list");

    resp[0] = '\0';
    rc = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

    TEST_CHECK(rc != RIG_OK);
    TEST_MSG("an unanswered command reported %s", rigerror(rc));

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* A reply that arrives while the caller is still being set up must not be
 * lost. The waiter is enrolled before the command goes out for this reason,
 * so the reader always has somewhere to put the answer. */
void test_a_reply_is_not_lost_when_it_arrives_at_once(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    int i;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "meter list", "IMMEDIATE");

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    /* Repeated, because losing this is a race and one attempt proves little. */
    for (i = 0; i < 20; i++)
    {
        char cmd[] = "meter list";
        char resp[128];
        int rc;

        resp[0] = '\0';
        rc = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

        TEST_CHECK(rc == RIG_OK && strstr(resp, "IMMEDIATE") != NULL);

        if (rc != RIG_OK || strstr(resp, "IMMEDIATE") == NULL)
        {
            TEST_MSG("attempt %d got \"%s\" (%s)", i, resp, rigerror(rc));
            break;
        }
    }

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* A reply that comes after the caller gave up has nowhere to go. The waiter
 * unlinks itself before returning, so the reader must find none and discard
 * the answer rather than write into a stack frame that is gone. */
void test_a_late_reply_is_discarded(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    char cmd[] = "meter list";
    char resp[128];
    int rc;
    freq_t freq = 0;

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);
    smartsdr_mock_reply(&m, "meter list", "TOOLATE");

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    /* Later than the caller is willing to wait. */
    smartsdr_mock_delay(&m, "meter list", 600);

    resp[0] = '\0';
    rc = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    TEST_CHECK(rc != RIG_OK);
    TEST_MSG("the caller should have given up, got %s", rigerror(rc));

    /* The answer lands about now, with no waiter to receive it. */
    smartsdr_mock_delay(&m, "", 0);
    usleep(700 * 1000);

    /* The session must still be usable, which it would not be if the late
     * reply had corrupted anything. */
    TEST_CHECK(rig_get_freq(rig, RIG_VFO_A, &freq) == RIG_OK);
    TEST_CHECK(freq > 0);
    TEST_MSG("the session did not survive a late reply");

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}


/* Status lines come off a network from a radio running firmware this code has
 * never seen, so the parser has to survive lines it cannot make sense of. What
 * must not happen is a half-read line recording a wrong value: a frequency read
 * as 0 is indistinguishable from a radio tuned to 0 Hz, and the operator has no
 * way to tell the two apart. Reads go through priv rather than rig_get_freq so
 * the frontend cache cannot mask what the parser did. */
void test_malformed_status_lines_leave_state_alone(void)
{
    struct smartsdr_mock m;
    RIG *rig;
    struct smartsdr_priv_data *priv;
    freq_t freq_before;
    rmode_t mode_before;
    int i;

    static const char *const junk[] =
    {
        "slice 0 RF_freq",                       /* truncated mid-field */
        "slice 0 in_use=1 RF_frequency=",        /* field present, no value */
        "slice 0 in_use=1 RF_frequency=banana",  /* value is not a number */
        "slice 0 in_use=1 mode=",                /* mode present, no value */
        "slice 0",                               /* no fields at all */
        "slice",                                 /* not even a slice number */
        "",                                      /* empty line */
    };

    smartsdr_mock_start(&m);
    TEST_ASSERT(m.started);

    rig = rig_on_mock(&m, RIG_MODEL_SMARTSDR);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        smartsdr_mock_stop(&m);
        TEST_ASSERT(0 && "rig_open against the mock failed");
    }

    priv = priv_of(rig);
    freq_before = smartsdr_slice_freq(priv);
    mode_before = smartsdr_slice_mode(priv);

    TEST_CHECK(freq_before == (freq_t)14100000);
    TEST_MSG("the rig did not pick up its own slice: freq=%.0f",
             (double)freq_before);

    for (i = 0; i < (int)(sizeof(junk) / sizeof(junk[0])); i++)
    {
        smartsdr_mock_push_status(&m, junk[i]);
        pump(rig);
    }

    pump(rig);

    TEST_CHECK(smartsdr_slice_freq(priv) == freq_before);
    TEST_MSG("a malformed line moved the frequency from %.0f to %.0f",
             (double)freq_before, (double)smartsdr_slice_freq(priv));

    TEST_CHECK(smartsdr_slice_mode(priv) == mode_before);
    TEST_MSG("a malformed line moved the mode from %s to %s",
             rig_strrmode(mode_before), rig_strrmode(smartsdr_slice_mode(priv)));

    /* A value that is present but not one this backend knows is not corruption:
     * it is the radio's actual state, and reporting the mode as unavailable is
     * more honest than reporting the last one that happened to parse. */
    smartsdr_mock_push_status(&m, "slice 0 in_use=1 mode=WOMBAT");

    for (i = 0; i < 40 && smartsdr_slice_mode(priv) == mode_before; i++)
    {
        pump(rig);
    }

    TEST_CHECK(smartsdr_slice_mode(priv) == RIG_MODE_NONE);
    TEST_MSG("an unknown mode should read as unavailable, got %s",
             rig_strrmode(smartsdr_slice_mode(priv)));

    /* And the session is still usable afterwards -- a parser that survives the
     * line but wedges the reader is no better. */
    smartsdr_mock_push_status(&m,
                              "slice 0 in_use=1 RF_frequency=21.200000 mode=CW");

    for (i = 0; i < 40 && smartsdr_slice_freq(priv) != (freq_t)21200000; i++)
    {
        pump(rig);
    }

    TEST_CHECK(smartsdr_slice_freq(priv) == (freq_t)21200000);
    TEST_MSG("the reader stopped absorbing after malformed input: freq=%.0f",
             (double)smartsdr_slice_freq(priv));

    rig_close(rig);
    rig_cleanup(rig);
    smartsdr_mock_stop(&m);
}

TEST_LIST =
{
    { "malformed_status_lines_leave_state_alone", test_malformed_status_lines_leave_state_alone },
    { "slice_create_empty_body_is_not_slice_zero", test_slice_create_empty_body_is_not_slice_zero },
    { "slice_missing_fail_does_not_create",   test_slice_missing_fail_does_not_create },
    { "status_for_another_slice_does_not_leak", test_status_for_another_slice_does_not_leak },
    { "meter_definitions_several_on_one_line", test_meter_definitions_several_on_one_line },
    { "meter_definition_with_zero_fps",       test_meter_definition_with_zero_fps },
    { "slice_create_body_names_the_created_slice", test_slice_create_body_names_the_created_slice },
    { "keepalive_timeout_disconnects_and_fails_open_streams", test_keepalive_timeout_disconnects_and_fails_open_streams },
    { "reconnect_reestablishes_and_restarts_keepalive", test_reconnect_reestablishes_and_restarts_keepalive },
    { "status_is_absorbed_with_no_command_in_flight", test_status_is_absorbed_with_no_command_in_flight },
    { "concurrent_callers_get_their_own_replies", test_concurrent_callers_get_their_own_replies },
    { "a_command_with_no_reply_times_out",    test_a_command_with_no_reply_times_out },
    { "a_reply_is_not_lost_when_it_arrives_at_once", test_a_reply_is_not_lost_when_it_arrives_at_once },
    { "a_late_reply_is_discarded",            test_a_late_reply_is_discarded },
    { "open_fails_when_a_step_is_refused",    test_open_fails_when_a_step_is_refused },
    { "slice_adopted_when_none_was_named",    test_slice_adopted_when_none_was_named },
    { "named_slice_is_not_swapped_for_another", test_named_slice_is_not_swapped_for_another },
    { NULL, NULL }
};
