/*
 *  Hamlib SmartSDR streaming tests
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

/* Integration tests for SmartSDR streaming against the simflex TCP/UDP simulator. */
/* Covers IQ RX after panafall/slice prep and pan=0 auto pan create. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "acutest.h"

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <hamlib/rig_state.h>
#include "smartsdr_props.h"
#include "smartsdr_rig.h"
#include "smartsdr_session.h"
#include "smartsdr_slice.h"
#include "smartsdr_stream.h"

#include <errno.h>

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <unistd.h>
#include <hamlib/rig.h>
/* Socket headers come from stream_proto.h, which picks the right set for the
 * host; do not include them directly. */
#include "../src/stream_proto.h"

#ifdef _WIN32

/* These tests drive a real simflex simulator child through fork()/exec, which
 * Windows does not provide, so the suite does not run there. The SmartSDR
 * stream code it exercises is also covered by test_smartsdr_vita and
 * test_smartsdr_session, which are portable. */
static void test_requires_posix_host(void)
{
    TEST_MSG("SmartSDR stream tests need fork(); skipped on this host");
    TEST_CHECK(1);
}

TEST_LIST =
{
    { "requires_posix_host", test_requires_posix_host },
    { NULL, NULL }
};

#else


struct simflex_proc
{
    pid_t pid;
    char logpath[128];   /* simflex stdout, so tests can assert on commands */
};


/* True if simflex logged a line containing needle. simflex prints one
 * "[cmd] ..." line per command it recognises. */
static int simflex_logged(const struct simflex_proc *proc, const char *needle)
{
    char line[1024];
    FILE *f;
    int found = 0;

    if (proc->logpath[0] == '\0')
    {
        return 0;
    }

    f = fopen(proc->logpath, "r");

    if (f == NULL)
    {
        return 0;
    }

    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (strstr(line, needle) != NULL)
        {
            found = 1;
            break;
        }
    }

    fclose(f);
    return found;
}


static const char *find_simflex(void)
{
    if (access("../simulators/simflex", X_OK) == 0)
    {
        return "../simulators/simflex";
    }

    if (access("../../simulators/simflex", X_OK) == 0)
    {
        return "../../simulators/simflex";
    }

    if (access("simulators/simflex", X_OK) == 0)
    {
        return "simulators/simflex";
    }

    return NULL;
}


static int wait_for_tcp_port(int port, int timeout_ms)
{
    int elapsed = 0;

    while (elapsed < timeout_ms)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;

        if (sock < 0)
        {
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((uint16_t)port);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            close(sock);
            return 0;
        }

        close(sock);
        hl_usleep(50000);
        elapsed += 50;
    }

    return -1;
}


/* Ephemeral port for simflex so parallel make -j check avoids clashes on 4992. */
static int pick_free_tcp_port(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;
    socklen_t len = sizeof(a);

    if (sock < 0)
    {
        return -1;
    }

    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = 0;

    if (bind(sock, (struct sockaddr *)&a, sizeof(a)) < 0)
    {
        close(sock);
        return -1;
    }

    if (getsockname(sock, (struct sockaddr *)&a, &len) < 0)
    {
        close(sock);
        return -1;
    }

    {
        int port = (int)ntohs(a.sin_port);
        close(sock);
        return port;
    }
}


static int start_simflex(struct simflex_proc *proc, int tcp_port)
{
    const char *path = find_simflex();
    char portstr[16];

    if (path == NULL)
    {
        return -1;
    }

    snprintf(portstr, sizeof(portstr), "%d", tcp_port);

    snprintf(proc->logpath, sizeof(proc->logpath),
             "simflex-%d.log", tcp_port);
    unlink(proc->logpath);

    proc->pid = fork();

    if (proc->pid < 0)
    {
        return -1;
    }

    if (proc->pid == 0)
    {
        if (freopen(proc->logpath, "w", stdout) != NULL)
        {
            setvbuf(stdout, NULL, _IOLBF, 0);
        }

        freopen("/dev/null", "w", stderr);
        execl(path, "simflex", portstr, (char *)NULL);
        _exit(127);
    }

    if (wait_for_tcp_port(tcp_port, 10000) != 0)
    {
        kill(proc->pid, SIGTERM);
        waitpid(proc->pid, NULL, 0);
        proc->pid = 0;
        return -1;
    }

    /* wait_for_tcp_port proves something answered, not that the simulator
     * survived it. Confirm the child is still running before handing the
     * port to the rig. */
    if (waitpid(proc->pid, NULL, WNOHANG) != 0)
    {
        proc->pid = 0;
        return -1;
    }

    return 0;
}


static void stop_simflex(struct simflex_proc *proc)
{
    if (proc->pid > 0)
    {
        kill(proc->pid, SIGTERM);
        waitpid(proc->pid, NULL, 0);
        proc->pid = 0;
    }

    /* The capture file is a test artefact; leaving it behind fails
     * distcleancheck. CLEANFILES covers the case where a test aborts
     * before reaching here. */
    if (proc->logpath[0] != '\0')
    {
        unlink(proc->logpath);
        proc->logpath[0] = '\0';
    }
}


/* do_prep: 1 = panafall + slice create before IQ open; 0 = pan=0 pan create path. */
static void iq_rx_body(int do_prep)
{
    struct simflex_proc sim = { 0 };
    RIG *rig = NULL;
    struct rig_stream_config cfg;
    rig_stream_t *stream = NULL;
    int ret;
    float buf[4096];
    size_t nread = 0;
    int nonzero = 0;
    int tcp_port = pick_free_tcp_port();
    char rigpath[64];

    if (tcp_port < 0)
    {
        TEST_ASSERT(0 && "pick_free_tcp_port failed");
    }

    if (start_simflex(&sim, tcp_port) < 0)
    {
        if (find_simflex() == NULL)
        {
            TEST_SKIP("simflex not built (expected at ../simulators/simflex "
                      "from build tree)");
        }

        TEST_ASSERT(0 && "simflex failed to listen on chosen TCP port");
    }

    rig = rig_init(RIG_MODEL_SMARTSDR_A);

    if (!rig)
    {
        stop_simflex(&sim);
        TEST_ASSERT(0 && "rig_init(SMARTSDR_A) failed");
    }

    snprintf(rigpath, sizeof(rigpath), "127.0.0.1:%d", tcp_port);
    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), rigpath);
    ret = rig_open(rig);

    if (ret != RIG_OK)
    {
        rig_cleanup(rig);
        stop_simflex(&sim);
        TEST_ASSERT(0 && "rig_open failed");
    }

    if (do_prep)
    {
        ret = smartsdr_prepare_slice_panafall(rig, 10.137);

        if (ret != RIG_OK)
        {
            rig_close(rig);
            rig_cleanup(rig);
            stop_simflex(&sim);
            TEST_CHECK(ret == RIG_OK);
            TEST_MSG("smartsdr_prepare_slice_panafall: %s", rigerror(ret));
            return;
        }
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);  /* same-build config */
    cfg.type = RIG_STREAM_TYPE_IQ_RX;
    cfg.format = RIG_STREAM_FORMAT_IQ_CF32;
    cfg.sample_rate = 48000;
    cfg.channels = 1;

    ret = rig_stream_open(rig, &cfg, &stream);

    if (ret != RIG_OK)
    {
        rig_close(rig);
        rig_cleanup(rig);
        stop_simflex(&sim);
        TEST_CHECK(ret == RIG_OK);
        TEST_MSG("rig_stream_open IQ_RX: %s", rigerror(ret));
        return;
    }

    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &nread, 3000, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_read ret=%d nread=%zu", ret, nread);
    TEST_CHECK(nread >= sizeof(float) * 4);

    if (nread >= sizeof(float))
    {
        int n = (int)(nread / sizeof(float));

        for (int i = 0; i < n; i++)
        {
            if (fabsf(buf[i]) > 1.0e-5f)
            {
                nonzero = 1;
                break;
            }
        }
    }

    TEST_CHECK(nonzero != 0);
    TEST_MSG("expected non-zero IQ samples, nread=%zu", nread);

    /* "Something arrived and was not zero" is what let a byte-swapped I/Q path
     * pass for so long: swapped counts are enormous rather than absent, and
     * swapped normalised samples are denormals that still read as non-zero
     * bit patterns. Hold the payload to the contract instead -- DAX I/Q is
     * little-endian counts against full scale 32768, so what reaches the
     * application must be inside +/-1.0 and must not be denormal. simflex
     * sends a half-scale tone, so the peak should be near 0.5. */
    {
        int n = (int)(nread / sizeof(float));
        int denormal = 0, out_of_range = 0, i;
        float peak = 0.0f;

        for (i = 0; i < n; i++)
        {
            float v = buf[i];

            if (v != 0.0f && fabsf(v) < 1.0e-37f) { denormal++; }

            if (fabsf(v) > 1.0f) { out_of_range++; }

            if (fabsf(v) > peak) { peak = fabsf(v); }
        }

        TEST_CHECK(denormal == 0);
        TEST_MSG("%d of %d samples denormal -- I/Q was byte-swapped", denormal, n);

        TEST_CHECK(out_of_range == 0);
        TEST_MSG("%d of %d samples outside +/-1.0 (peak %.6g) -- I/Q was not "
                 "normalised, or was byte-swapped", out_of_range, n, peak);

        TEST_CHECK(peak > 0.4f && peak < 0.6f);
        TEST_MSG("peak %.6g, expected about 0.5 for simflex's half-scale tone",
                 peak);
    }

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    stop_simflex(&sim);
}


/* Open a DAX audio RX stream and require real samples. The remote_audio_rx
 * command carries no ip=/port=, so this path depends entirely on the client
 * having registered its UDP port with "client udpport". */
static void audio_rx_body(void)
{
    struct simflex_proc sim = { 0 };
    RIG *rig = NULL;
    struct rig_stream_config cfg;
    rig_stream_t *stream = NULL;
    int ret;
    float buf[4096];
    size_t nread = 0;
    int tcp_port = pick_free_tcp_port();
    char rigpath[64];

    if (tcp_port < 0)
    {
        TEST_ASSERT(0 && "pick_free_tcp_port failed");
    }

    if (start_simflex(&sim, tcp_port) < 0)
    {
        if (find_simflex() == NULL)
        {
            TEST_SKIP("simflex not built (expected at ../simulators/simflex "
                      "from build tree)");
        }

        TEST_ASSERT(0 && "simflex failed to listen on chosen TCP port");
    }

    rig = rig_init(RIG_MODEL_SMARTSDR_A);

    if (!rig)
    {
        stop_simflex(&sim);
        TEST_ASSERT(0 && "rig_init(SMARTSDR_A) failed");
    }

    snprintf(rigpath, sizeof(rigpath), "127.0.0.1:%d", tcp_port);
    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), rigpath);
    ret = rig_open(rig);

    if (ret != RIG_OK)
    {
        rig_cleanup(rig);
        stop_simflex(&sim);
        TEST_ASSERT(0 && "rig_open failed");
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.struct_size = sizeof(cfg);
    cfg.type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg.format = RIG_STREAM_FORMAT_PCM_F32;
    cfg.sample_rate = 24000;
    cfg.channels = 2;

    ret = rig_stream_open(rig, &cfg, &stream);

    if (ret != RIG_OK)
    {
        rig_close(rig);
        rig_cleanup(rig);
        stop_simflex(&sim);
        TEST_CHECK(ret == RIG_OK);
        TEST_MSG("rig_stream_open AUDIO_RX: %s", rigerror(ret));
        return;
    }

    ret = rig_stream_read(rig, stream, buf, sizeof(buf), &nread, 3000, NULL);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("rig_stream_read ret=%d nread=%zu", ret, nread);
    TEST_CHECK(nread >= sizeof(float) * 4);
    TEST_MSG("expected audio samples, nread=%zu", nread);

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    stop_simflex(&sim);
}


void test_audio_rx_lan(void)
{
    audio_rx_body();
}


/* Same as above against a radio that reports remote_on_enabled=1. That flag
 * describes the radio's SmartLink configuration, not this connection, so a LAN
 * client must still register its UDP port the LAN way and still get audio. */
void test_audio_rx_smartlink_radio(void)
{
#if defined(_WIN32) || defined(__WIN32__)
    TEST_SKIP("setenv not used on this platform");
#else
    int er = setenv("SIMFLEX_REMOTE_ON", "1", 1);

    if (er != 0)
    {
        TEST_CHECK(er == 0);
        TEST_MSG("setenv SIMFLEX_REMOTE_ON: %s", strerror(errno));
        return;
    }

    audio_rx_body();
    unsetenv("SIMFLEX_REMOTE_ON");
#endif
}


/* Status for a slice this rig is not bound to must not alter its cached
 * frequency, mode, width or PTT. Confirmed against hardware: with slice A on
 * 14.100 USB and slice B on 7.100 LSB, model 23005 reported slice B's values. */
void test_parse_status_other_slice_does_not_leak(void)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR_A);
    struct smartsdr_priv_data *priv;
    freq_t freq_before;
    rmode_t mode_before;
    int ptt_before;
    char other[512];
    char own[512];

    if (!rig)
    {
        TEST_ASSERT(0 && "rig_init(SMARTSDR_A) failed");
    }

    priv = (struct smartsdr_priv_data *)
           ((struct rig_state *)rig_data_pointer(rig, RIG_PTRX_STATE))->priv;

    /* Seed this rig's own slice (0) state. */
    snprintf(own, sizeof(own),
             "S1234ABCD|slice 0 in_use=1 RF_frequency=14.100000 "
             "index_letter=A mode=USB filter_hi=2800 state=RECEIVE tx=1");
    smartsdr_parse_status_line(rig, own);

    freq_before = smartsdr_slice_freq(priv);
    mode_before = smartsdr_slice_mode(priv);
    ptt_before = smartsdr_slice_ptt(priv);

    TEST_CHECK(freq_before == (freq_t)14100000);
    TEST_MSG("own slice seed: freq=%.0f expected 14100000",
             (double)freq_before);

    /* A different slice reports a different frequency, mode and TX state. */
    snprintf(other, sizeof(other),
             "S1234ABCD|slice 1 in_use=1 RF_frequency=7.100000 "
             "index_letter=B mode=LSB filter_hi=3000 state=TRANSMITTING tx=1");
    smartsdr_parse_status_line(rig, other);

    TEST_CHECK(smartsdr_slice_freq(priv) == freq_before);
    TEST_MSG("frequency leaked: %.0f (expected %.0f)",
             (double)smartsdr_slice_freq(priv), (double)freq_before);

    TEST_CHECK(smartsdr_slice_mode(priv) == mode_before);
    TEST_MSG("mode leaked: %s (expected %s)",
             rig_strrmode(smartsdr_slice_mode(priv)), rig_strrmode(mode_before));

    TEST_CHECK(smartsdr_slice_ptt(priv) == ptt_before);
    TEST_MSG("ptt leaked: %d (expected %d)",
             smartsdr_slice_ptt(priv), ptt_before);

    rig_cleanup(rig);
}


/* Open a rig against a fresh simflex. Caller must rig_close/rig_cleanup and
 * stop_simflex. Returns NULL (already cleaned up) if the sim cannot start. */
static RIG *open_rig_on_simflex(struct simflex_proc *sim)
{
    RIG *rig;
    int tcp_port = pick_free_tcp_port();
    char rigpath[64];

    if (tcp_port < 0)
    {
        TEST_ASSERT(0 && "pick_free_tcp_port failed");
    }

    if (start_simflex(sim, tcp_port) < 0)
    {
        if (find_simflex() == NULL)
        {
            TEST_SKIP("simflex not built");
        }

        TEST_ASSERT(0 && "simflex failed to listen");
    }

    rig = rig_init(RIG_MODEL_SMARTSDR_A);

    if (!rig)
    {
        stop_simflex(sim);
        TEST_ASSERT(0 && "rig_init(SMARTSDR_A) failed");
    }

    snprintf(rigpath, sizeof(rigpath), "127.0.0.1:%d", tcp_port);
    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), rigpath);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        stop_simflex(sim);
        TEST_ASSERT(0 && "rig_open failed");
    }

    return rig;
}


/* An unsupported mode must be rejected, not formatted into the command with a
 * NULL mode string. WFM is absent from the radio's mode_list. */
void test_set_mode_unsupported_is_rejected(void)
{
    struct simflex_proc sim = { 0 };
    RIG *rig = open_rig_on_simflex(&sim);
    int ret;

    if (!rig) { return; }

    ret = rig_set_mode(rig, RIG_VFO_A, RIG_MODE_WFM, 2400);

    TEST_CHECK(ret != RIG_OK);
    TEST_MSG("set_mode(WFM) returned %d (%s), expected an error",
             ret, rigerror(ret));

    TEST_CHECK(!simflex_logged(&sim, "mode=(null)"));
    TEST_MSG("a NULL mode string reached the wire");

    rig_close(rig);
    rig_cleanup(rig);
    stop_simflex(&sim);
}


/* A passband width must actually be sent to the radio. */
void test_set_mode_width_is_sent(void)
{
    struct simflex_proc sim = { 0 };
    RIG *rig = open_rig_on_simflex(&sim);
    int ret;

    if (!rig) { return; }

    ret = rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USB, 2400);
    TEST_CHECK(ret == RIG_OK);
    TEST_MSG("set_mode(USB, 2400) returned %d (%s)", ret, rigerror(ret));

    TEST_CHECK(simflex_logged(&sim, "filt"));
    TEST_MSG("no filter command reached the radio for width=2400");

    rig_close(rig);
    rig_cleanup(rig);
    stop_simflex(&sim);
}


/* Properties reported once in a long status line must survive the stream of
 * short update lines that follows. An earlier text-retaining cache evicted
 * them, so reads started failing after ordinary traffic. */
void test_props_survive_status_churn(void)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR_A);
    struct smartsdr_priv_data *priv;
    char line[2048];
    int i;

    if (!rig)
    {
        TEST_ASSERT(0 && "rig_init(SMARTSDR_A) failed");
    }

    priv = (struct smartsdr_priv_data *)
           ((struct rig_state *)rig_data_pointer(rig, RIG_PTRX_STATE))->priv;

    /* One full slice line, carrying properties the radio reports only here. */
    snprintf(line, sizeof(line),
             "S1234ABCD|slice 0 in_use=1 RF_frequency=14.100000 index_letter=A "
             "rxant=ANT1 txant=ANT1 mode=USB audio_level=50 audio_mute=0 "
             "squelch=0 squelch_level=20 agc_mode=med nr=0 nr_level=50 "
             "ant_list=ANT1,ANT2,RX_A,XVTA tx_ant_list=ANT1,ANT2,XVTA");
    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, line);

    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].seen);
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].ival == 50);
    TEST_CHECK(priv->props[SMARTSDR_P_TXANT].seen);

    /* Many short updates, none of which mention those properties. */
    for (i = 0; i < 40; i++)
    {
        snprintf(line, sizeof(line), "S1234ABCD|slice 0 qsk=%d", i & 1);
        smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, line);
    }

    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].seen);
    TEST_MSG("audio_level was lost after status churn");
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].ival == 50);
    TEST_MSG("audio_level changed to %d", priv->props[SMARTSDR_P_AUDIO_LEVEL].ival);

    TEST_CHECK(priv->props[SMARTSDR_P_TXANT].seen);
    TEST_MSG("txant was lost after status churn");
    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_TXANT].sval, "ANT1") == 0);
    TEST_MSG("txant became '%s'", priv->props[SMARTSDR_P_TXANT].sval);

    TEST_CHECK(strcmp(priv->props[SMARTSDR_P_TX_ANT_LIST].sval,
                      "ANT1,ANT2,XVTA") == 0);
    TEST_MSG("tx_ant_list became '%s'", priv->props[SMARTSDR_P_TX_ANT_LIST].sval);

    /* A short line that does carry a tracked property still updates it. */
    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE,
                          "S1234ABCD|slice 0 audio_level=73");
    TEST_CHECK(priv->props[SMARTSDR_P_AUDIO_LEVEL].ival == 73);
    TEST_MSG("short update did not apply: %d",
             priv->props[SMARTSDR_P_AUDIO_LEVEL].ival);

    rig_cleanup(rig);
}


/* DAX-IQ is referenced to the panadapter centre, not the slice frequency.
 * Our own tuning passes autopan=1 so they normally track, but the operator or
 * another client can move the pan, and then they differ - reproduced on a
 * FLEX-8400M with pan centre 14.150 while the slice stayed on 14.100. */
void test_pan_center_tracked_separately_from_slice(void)
{
    RIG *rig = rig_init(RIG_MODEL_SMARTSDR_A);
    struct smartsdr_priv_data *priv;
    char line[512];

    if (!rig)
    {
        TEST_ASSERT(0 && "rig_init(SMARTSDR_A) failed");
    }

    priv = (struct smartsdr_priv_data *)
           ((struct rig_state *)rig_data_pointer(rig, RIG_PTRX_STATE))->priv;

    /* Bind this rig's slice to a panadapter. */
    snprintf(line, sizeof(line),
             "S1234ABCD|slice 0 in_use=1 RF_frequency=14.100000 index_letter=A "
             "mode=USB pan=0x40000000 filter_lo=100 filter_hi=2800");
    smartsdr_parse_status_line(rig, line);
    TEST_CHECK(priv->slice_pan_id == 0x40000000U);

    /* The pan reports its own centre, away from the slice. */
    snprintf(line, sizeof(line),
             "S1234ABCD|display pan 0x40000000 x_pixels=50 y_pixels=20 "
             "center=14.150000 bandwidth=0.200000");
    smartsdr_parse_status_line(rig, line);

    TEST_CHECK(priv->pan_center_hz == 14150000.0);
    TEST_MSG("pan centre tracked as %.0f, expected 14150000",
             priv->pan_center_hz);

    /* The slice frequency must be unaffected by the pan moving. */
    TEST_CHECK(smartsdr_slice_freq(priv) == (freq_t)14100000);
    TEST_MSG("slice frequency became %.0f",
             (double)smartsdr_slice_freq(priv));

    /* A pan this rig is not bound to must not move our centre. */
    snprintf(line, sizeof(line),
             "S1234ABCD|display pan 0x40000009 center=7.050000 bandwidth=0.2");
    smartsdr_parse_status_line(rig, line);
    TEST_CHECK(priv->pan_center_hz == 14150000.0);
    TEST_MSG("another pan's centre leaked in: %.0f", priv->pan_center_hz);

    rig_cleanup(rig);
}


/* When the radio goes away, an open stream must fail rather than time out
 * forever, and the reason must reach the application.
 *
 * The loss is signalled directly rather than by killing the simulator: whether
 * a write to a dead socket fails on the first attempt or the third is the
 * operating system's business, not this backend's, and depending on it makes
 * the test flaky. What is tested here is what the backend does once it knows. */
void test_session_loss_fails_the_stream(void)
{
    struct simflex_proc sim = { 0 };
    RIG *rig = NULL;
    struct rig_stream_config cfg;
    rig_stream_t *stream = NULL;
    struct rig_state *rs;
    float buf[2048];
    size_t nread = 0;
    int ret;
    int attempt;
    int tcp_port = pick_free_tcp_port();
    char rigpath[64];

    if (tcp_port < 0)
    {
        TEST_ASSERT(0 && "pick_free_tcp_port failed");
    }

    if (start_simflex(&sim, tcp_port) < 0)
    {
        if (find_simflex() == NULL)
        {
            TEST_SKIP("simflex not built");
        }

        TEST_ASSERT(0 && "simflex failed to listen");
    }

    rig = rig_init(RIG_MODEL_SMARTSDR_A);

    if (!rig)
    {
        stop_simflex(&sim);
        TEST_ASSERT(0 && "rig_init(SMARTSDR_A) failed");
    }

    snprintf(rigpath, sizeof(rigpath), "127.0.0.1:%d", tcp_port);
    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), rigpath);

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        stop_simflex(&sim);
        TEST_ASSERT(0 && "rig_open failed");
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
        stop_simflex(&sim);
        TEST_ASSERT(0 && "rig_stream_open AUDIO_RX failed");
    }

    rs = (struct rig_state *)rig_data_pointer(rig, RIG_PTRX_STATE);

    smartsdr_session_lost(rig, RIG_COMM_REASON_LINK_TIMEOUT);

    TEST_CHECK(rs->comm_status == RIG_COMM_STATUS_DISCONNECTED);
    TEST_MSG("comm_status is %d, expected DISCONNECTED", (int)rs->comm_status);

    TEST_CHECK(rs->comm_reason == RIG_COMM_REASON_LINK_TIMEOUT);
    TEST_MSG("comm_reason is %s, expected LINK_TIMEOUT",
             rig_strcommreason(rs->comm_reason));

    /* Samples buffered before the loss are still valid and are delivered
     * first, and the RX thread needs a moment to notice, so poll. */
    ret = RIG_OK;

    for (attempt = 0; attempt < 20 && ret != -RIG_EIO; attempt++)
    {
        nread = 0;
        ret = rig_stream_read(rig, stream, buf, sizeof(buf), &nread, 500, NULL);
    }

    TEST_CHECK(ret == -RIG_EIO);
    TEST_MSG("stream read returned %d (%s), expected -RIG_EIO", ret,
             rigerror(ret));

    /* A second loss must not overwrite the first reason, which is the one
     * that explains what happened. */
    smartsdr_session_lost(rig, RIG_COMM_REASON_SOCKET_ERROR);
    TEST_CHECK(rs->comm_reason == RIG_COMM_REASON_LINK_TIMEOUT);
    TEST_MSG("reason was overwritten with %s",
             rig_strcommreason(rs->comm_reason));

    rig_stream_close(rig, stream);
    rig_close(rig);
    rig_cleanup(rig);
    stop_simflex(&sim);
}


void test_iq_rx_slice_panafall_prep_then_iq(void)
{
    iq_rx_body(1);
}


void test_iq_rx_panadapter_and_daxiq(void)
{
    iq_rx_body(0);
}


void test_mode_status_usb_string(void)
{
    rmode_t m;
    int r = smartsdr_parse_mode_status_value("USB", &m);

    TEST_CHECK(r == 0);
    TEST_CHECK(m == RIG_MODE_USB);
}


void test_mode_status_numeric_one_is_usb(void)
{
    rmode_t m;
    int r = smartsdr_parse_mode_status_value("1", &m);

    TEST_CHECK(r == 0);
    TEST_CHECK(m == RIG_MODE_USB);
}


void test_mode_status_lowercase_digu(void)
{
    rmode_t m;
    int r = smartsdr_parse_mode_status_value("digu", &m);

    TEST_CHECK(r == 0);
    TEST_CHECK(m == RIG_MODE_PKTUSB);
}


void test_iq_rx_force_wan_env_udp_register(void)
{
#if defined(_WIN32) || defined(__WIN32__)
    TEST_SKIP("setenv not used on this platform for WAN test");
#else
    int er = setenv("HAMLIB_SMARTSDR_WAN", "1", 1);

    if (er != 0)
    {
        TEST_CHECK(er == 0);
        TEST_MSG("setenv HAMLIB_SMARTSDR_WAN: %s", strerror(errno));
        return;
    }

    iq_rx_body(0);
    unsetenv("HAMLIB_SMARTSDR_WAN");
#endif
}


/* A radio with SmartLink configured reports remote_on_enabled=1 to every
 * client, LAN clients included. That flag describes the radio, not the
 * connection, so a LAN client must still register its UDP port the LAN way
 * and must still receive data. */
void test_iq_rx_smartlink_radio_uses_lan_path(void)
{
#if defined(_WIN32) || defined(__WIN32__)
    TEST_SKIP("setenv not used on this platform");
#else
    int er = setenv("SIMFLEX_REMOTE_ON", "1", 1);

    if (er != 0)
    {
        TEST_CHECK(er == 0);
        TEST_MSG("setenv SIMFLEX_REMOTE_ON: %s", strerror(errno));
        return;
    }

    iq_rx_body(1);
    unsetenv("SIMFLEX_REMOTE_ON");
#endif
}


/* Build consecutive real VITA-49 frames and parse them back, asserting
 * packet_type, stream_id, and a monotonically increasing 4-bit packet_count
 * across the sequence. The integration path delivers already-decoded float
 * samples, so this exercises the wire frame directly via the public codec. */
void test_vita49_consecutive_frames_monotonic(void)
{
    const uint32_t stream_id = 0x04000123;
    const uint64_t class_id = ((uint64_t)0x1C2D << 48)
                              | (uint64_t)VITA49_PCC_AUDIO_F32;
    const float samples[4] = { 0.25f, -0.5f, 0.75f, -1.0f };

    uint8_t prev_count = 0;
    int first = 1;
    int i;

    /* The packet_count is a 4-bit field, so it wraps modulo 16. Walk past
     * the wrap to confirm the parsed value tracks the built value exactly. */
    for (i = 0; i < 20; i++)
    {
        uint8_t pc = (uint8_t)(i & 0x0F);
        uint8_t pkt[64];

        int pkt_len = vita49_build_tx_packet(pkt, sizeof(pkt), stream_id,
                                             class_id, pc,
                                             samples, 4);
        TEST_CHECK(pkt_len == VITA49_HEADER_BYTES + 4 * (int)sizeof(float));
        TEST_MSG("frame %d: build returned %d", i, pkt_len);

        struct vita49_header hdr;
        int rc = vita49_parse_header(pkt, pkt_len, &hdr);
        TEST_CHECK(rc == 0);

        TEST_CHECK(hdr.packet_type == VITA49_PKT_TYPE_IF_DATA);
        TEST_MSG("frame %d: packet_type=%u expected %u",
                 i, hdr.packet_type, VITA49_PKT_TYPE_IF_DATA);

        TEST_CHECK(hdr.stream_id == stream_id);
        TEST_MSG("frame %d: stream_id=0x%x expected 0x%x",
                 i, hdr.stream_id, stream_id);

        TEST_CHECK(hdr.pcc == VITA49_PCC_AUDIO_F32);
        TEST_MSG("frame %d: pcc=0x%x expected 0x%x",
                 i, hdr.pcc, VITA49_PCC_AUDIO_F32);

        /* The parsed counter must equal the value we put in. */
        TEST_CHECK(hdr.packet_count == pc);
        TEST_MSG("frame %d: packet_count=%u expected %u",
                 i, hdr.packet_count, pc);

        /* Consecutive frames advance by exactly one in the 4-bit field
         * (modulo 16), i.e. monotonic with defined wrap. */
        if (!first)
        {
            uint8_t expected_next = (uint8_t)((prev_count + 1) & 0x0F);
            TEST_CHECK(hdr.packet_count == expected_next);
            TEST_MSG("frame %d: packet_count=%u expected next %u (prev %u)",
                     i, hdr.packet_count, expected_next, prev_count);
        }

        prev_count = hdr.packet_count;
        first = 0;
    }
}


/* The radio's mode spellings are needed in both directions, and the two used to
 * be separate lists. This pins them to each other: every spelling the backend
 * will send must parse back to the mode it names, so a table edit that breaks
 * the round trip fails here rather than on the air. */
void test_mode_names_round_trip(void)
{
    static const rmode_t emitted[] =
    {
        RIG_MODE_CW, RIG_MODE_USB, RIG_MODE_LSB, RIG_MODE_PKTUSB,
        RIG_MODE_PKTLSB, RIG_MODE_AM, RIG_MODE_FM, RIG_MODE_FMN,
        RIG_MODE_SAM, RIG_MODE_RTTY
    };
    size_t i;

    for (i = 0; i < sizeof(emitted) / sizeof(emitted[0]); i++)
    {
        const char *name = smartsdr_mode_name(emitted[i]);
        rmode_t back = RIG_MODE_NONE;

        TEST_CHECK(name != NULL);
        TEST_MSG("no spelling for mode %s", rig_strrmode(emitted[i]));

        if (name == NULL) { continue; }

        TEST_CHECK(smartsdr_parse_mode_status_value(name, &back) == 0);
        TEST_MSG("the backend sends \"%s\" but cannot parse it back", name);

        TEST_CHECK(back == emitted[i]);
        TEST_MSG("\"%s\" round-tripped to %s, expected %s", name,
                 rig_strrmode(back), rig_strrmode(emitted[i]));
    }
}

/* Spellings the radio reports that are not what the backend sends back. These
 * must parse, and must not become the emitted name for their mode -- NFM and
 * FMN are the same mode, and sending the wrong one is a command the radio
 * rejects. */
void test_mode_aliases_parse_but_are_not_emitted(void)
{
    rmode_t m = RIG_MODE_NONE;

    TEST_CHECK(smartsdr_parse_mode_status_value("NFM", &m) == 0 && m == RIG_MODE_FMN);
    TEST_MSG("NFM should read as FMN, got %s", rig_strrmode(m));

    /* Hamlib has no DFM, so the radio's DFM is FM. */
    m = RIG_MODE_NONE;
    TEST_CHECK(smartsdr_parse_mode_status_value("DFM", &m) == 0 && m == RIG_MODE_FM);
    TEST_MSG("DFM should read as FM, got %s", rig_strrmode(m));

    TEST_CHECK(strcmp(smartsdr_mode_name(RIG_MODE_FMN), "FMN") == 0);
    TEST_MSG("FMN must be sent as FMN, not as its alias");
}

/* Status lines are not case-normalised by the radio, and an unknown token must
 * be refused rather than silently read as some default mode. */
void test_mode_parsing_is_case_insensitive_and_rejects_junk(void)
{
    rmode_t m = RIG_MODE_NONE;

    TEST_CHECK(smartsdr_parse_mode_status_value("UsB", &m) == 0 && m == RIG_MODE_USB);
    TEST_MSG("mixed case should parse, got %s", rig_strrmode(m));

    m = RIG_MODE_NONE;
    TEST_CHECK(smartsdr_parse_mode_status_value("  lsb", &m) == 0 && m == RIG_MODE_LSB);
    TEST_MSG("leading space should be tolerated, got %s", rig_strrmode(m));

    /* A prefix of a real spelling is not that spelling. */
    m = RIG_MODE_NONE;
    TEST_CHECK(smartsdr_parse_mode_status_value("US", &m) != 0);
    TEST_MSG("\"US\" must not be accepted as USB");

    m = RIG_MODE_NONE;
    TEST_CHECK(smartsdr_parse_mode_status_value("USBB", &m) != 0);
    TEST_MSG("\"USBB\" must not be accepted as USB");

    m = RIG_MODE_NONE;
    TEST_CHECK(smartsdr_parse_mode_status_value("", &m) != 0);
    TEST_CHECK(smartsdr_parse_mode_status_value("wombat", &m) != 0);
}

TEST_LIST =
{
    { "mode_names_round_trip", test_mode_names_round_trip },
    { "mode_aliases_parse_but_are_not_emitted", test_mode_aliases_parse_but_are_not_emitted },
    { "mode_parsing_is_case_insensitive_and_rejects_junk", test_mode_parsing_is_case_insensitive_and_rejects_junk },
    { "vita49_consecutive_frames_monotonic", test_vita49_consecutive_frames_monotonic },
    { "iq_rx_slice_panafall_prep_then_iq", test_iq_rx_slice_panafall_prep_then_iq },
    { "iq_rx_panadapter_and_daxiq", test_iq_rx_panadapter_and_daxiq },
    { "mode_status_usb_string", test_mode_status_usb_string },
    { "mode_status_numeric_one_is_usb", test_mode_status_numeric_one_is_usb },
    { "mode_status_lowercase_digu", test_mode_status_lowercase_digu },
    { "iq_rx_force_wan_env_udp_register", test_iq_rx_force_wan_env_udp_register },
    { "iq_rx_smartlink_radio_uses_lan_path", test_iq_rx_smartlink_radio_uses_lan_path },
    { "audio_rx_lan", test_audio_rx_lan },
    { "audio_rx_smartlink_radio", test_audio_rx_smartlink_radio },
    { "parse_status_other_slice_does_not_leak", test_parse_status_other_slice_does_not_leak },
    { "set_mode_unsupported_is_rejected", test_set_mode_unsupported_is_rejected },
    { "props_survive_status_churn", test_props_survive_status_churn },
    { "session_loss_fails_the_stream", test_session_loss_fails_the_stream },
    { "pan_center_tracked_separately_from_slice", test_pan_center_tracked_separately_from_slice },
    { "set_mode_width_is_sent", test_set_mode_width_is_sent },
    { NULL, NULL }
};

#endif  /* _WIN32 */
