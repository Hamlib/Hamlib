/*
 *  Hamlib streaming test tool
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

/* Test tool for exercising the Hamlib streaming API against real hardware. */
/* Opens a rig, starts audio/IQ streams, reads samples, and prints stats. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include "token.h"
#include "misc.h"
#include "rigstreamtest_util.h"
#include "stream_proto.h"


static volatile sig_atomic_t running = 1;
/* --require-native[=LIST]: conversion stages the open must not install */
static uint32_t g_require_native = RIG_STREAM_CONV_NONE;

/* Report the frontend conversion stages of a freshly opened stream:
 * CONV_NONE marks a native stream (bytes untouched). */
static void report_conversions(rig_stream_t *st, const char *tag)
{
    int c = rig_stream_get_conversions(st);
    char stages[64];

    stream_conversions_str(c, stages, sizeof(stages));

    printf("Stream %s: conversions=0x%x (%s)\n", tag, c,
           c == RIG_STREAM_CONV_NONE ? "native stream" : stages);
}

static void sighandler(int sig)
{
    (void)sig;
    running = 0;
}


/* Pace a real-time sample producer against an absolute monotonic schedule:
 * sample-frame n is due at start + n/sample_rate, so per-iteration overhead
 * and sleep granularity can never accumulate into a rate deficit. Sleeps only
 * when ahead of schedule; when behind, returns immediately to catch up. */
static void pace_to_frame(const struct timespec *start,
                          uint64_t frames_written, int sample_rate)
{
    struct timespec now;
    int64_t start_ns, now_ns, target_ns, wait_ns;

    clock_gettime(CLOCK_MONOTONIC, &now);
    start_ns = (int64_t)start->tv_sec * 1000000000LL + start->tv_nsec;
    now_ns = (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec;
    target_ns = start_ns
                + (int64_t)(frames_written * 1000000000ULL
                            / (uint64_t)sample_rate);
    wait_ns = target_ns - now_ns;

    if (wait_ns > 0)
    {
        hl_usleep((unsigned long)(wait_ns / 1000));
    }
}


static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  -m, --model <id>       Rig model number (required)\n"
            "  -r, --rig-file <host>  IP address or hostname[:port] of radio\n"
            "  -t, --type <type>      Stream type: audio_rx (default), audio_tx,\n"
            "                         iq_rx, iq_tx, loopback\n"
            "  -s, --sample-rate <hz> Sample rate in Hz (default: 24000)\n"
            "  -c, --channels <n>     Number of channels, 1 or 2 (default: 2)\n"
            "  -C, --set-conf <k=v,.> Set backend config tokens (comma-separated)\n"
            "  -d, --duration <sec>   Run duration in seconds (0 = until Ctrl-C)\n"
            "  -F, --frame-ms <ms>    TX write size in milliseconds, 1..500\n"
            "                         (default: 20); frame samples scale with\n"
            "                         the sample rate; bounded by the stream\n"
            "                         ring capacity\n"
            "  -w, --wav <file>       Write RX audio to WAV file (16-bit PCM)\n"
            "  -o, --iq-out <file>    Write IQ_RX to raw native-endian float32 I/Q\n"
            "  -P, --ptt              TX phases: key PTT (TRANSMITS RF!)\n"
            "  -v, --verbose          Increase debug verbosity (repeat for more)\n"
            "  -h, --help             Show this help\n"
            "\n"
            "Alternating RX/TX soak (active when --rx-secs or --tx-secs given;\n"
            "overrides -t; -d/--duration caps the whole run):\n"
            "  --rx-secs <sec>        RX phase seconds per cycle (0 = no RX)\n"
            "  --tx-secs <sec>        TX phase seconds per cycle (0 = RX-only)\n"
            "  --cycles <n>           Number of cycles (0 = until -d / Ctrl-C)\n"
            "  --power <0.0..1.0>     RF power for TX phases (sets RFPOWER level)\n"
            "  --iq                   Use I/Q streams for the phases (default: audio)\n"
            "  --stats-secs <sec>     Interim stats interval (default 5)\n"
            "  --buffer-ms <ms>       Ring buffer size as ms of stream data\n"
            "  --require-native[=LIST] Conversion stages the stream must NOT\n"
            "                         use: comma-separated FORMAT, RATE,\n"
            "                         CHANNELS, or ALL (the default when the\n"
            "                         flag is given bare). An open needing a\n"
            "                         listed stage fails instead of converting;\n"
            "                         stages left out are still converted, so\n"
            "                         --require-native=RATE demands a native\n"
            "                         sample rate but accepts a format change\n"
            "\n"
            "Full-duplex soak (--full-duplex): RX and TX streams run concurrently\n"
            "the whole run; PTT is cycled ON for --tx-secs / OFF for --rx-secs\n"
            "(--rx-secs 0 = always keyed, --tx-secs 0 = never keyed; -P gates RF):\n"
            "  --full-duplex          Enable full-duplex mode\n"
            "  --tone-on-ms <ms>      TX tone ON duration (0/0 = tone always on)\n"
            "  --tone-off-ms <ms>     TX tone OFF (silence) duration; free-running,\n"
            "                         independent of PTT\n"
            "\n"
            "Examples:\n"
            "  %s -m 23005 -r 192.168.1.100 -t audio_rx -d 10\n"
            "  %s -m 23005 -r 192.168.1.100 -t audio_rx -d 10 -w capture.wav\n"
            "  %s -m 23005 -r 192.168.1.100 -t loopback -d 5\n"
            "  %s -m 23005 -r 192.168.1.100 -t iq_rx -s 48000 -d 10\n"
            "  %s -m 23005 -r 192.168.1.100 -t iq_rx -p -s 48000 -d 10\n"
            "  %s -m 23005 -r 192.168.1.100 -t iq_rx -s 48000 -d 10 -o iq.f32\n"
            "  %s -m 23005 -r 192.168.1.100 -t iq_tx -s 48000 -p -d 5\n",
            prog, prog, prog, prog, prog, prog, prog, prog);
}


static const char *type_name(rig_stream_type_t type)
{
    switch (type)
    {
    case RIG_STREAM_TYPE_AUDIO_RX: return "AUDIO_RX";

    case RIG_STREAM_TYPE_AUDIO_TX: return "AUDIO_TX";

    case RIG_STREAM_TYPE_IQ_RX:    return "IQ_RX";

    case RIG_STREAM_TYPE_IQ_TX:    return "IQ_TX";

    default:                   return "UNKNOWN";
    }
}


/* Print per-second statistics for a running RX stream. */
static void print_rx_stats(RIG *rig, rig_stream_t *stream,
                           uint64_t total_bytes, int elapsed_sec)
{
    struct rig_stream_stats st;
    memset(&st, 0, sizeof(st));
    rig_stream_get_stats(rig, stream, &st);

    printf("[%4ds] bytes=%llu  gaps=%u(%u unsized)  overruns=%u  "
           "underruns=%u  link=%u  dropped(gap/ovr/link)=%llu/%llu/%llu\n",
           elapsed_sec,
           (unsigned long long)total_bytes,
           st.gaps, st.gaps_unknown, st.overruns, st.underruns, st.link_loss,
           (unsigned long long)st.dropped_samples_gap,
           (unsigned long long)st.dropped_samples_overrun,
           (unsigned long long)st.dropped_samples_link);
    fflush(stdout);
}


/* Underrun count via the stats snapshot (TX progress lines). */
static unsigned stream_underruns(RIG *rig, rig_stream_t *stream)
{
    struct rig_stream_stats st;
    memset(&st, 0, sizeof(st));
    rig_stream_get_stats(rig, stream, &st);
    return st.underruns;
}


/* Print peak/RMS amplitude for a chunk of float32 samples. */
static void print_audio_level(const float *samples, int num_samples)
{
    float peak = 0.0f;
    double sum_sq = 0.0;

    for (int i = 0; i < num_samples; i++)
    {
        float abs_val = fabsf(samples[i]);

        if (abs_val > peak)
        {
            peak = abs_val;
        }

        sum_sq += (double)samples[i] * (double)samples[i];
    }

    float rms = (num_samples > 0) ? (float)sqrt(sum_sq / num_samples) : 0.0f;
    printf("  audio: peak=%.6f  rms=%.6f\n", peak, rms);
}


/* Run an RX stream: read samples, print statistics, optionally write WAV. */
static int run_rx_single(RIG *rig, rig_stream_type_t type,
                         int sample_rate, int channels, int duration_sec,
                         const char *wav_path, const char *iq_raw_path)
{
    struct rig_stream_config *config = rig_stream_config_alloc();
    rig_stream_t *stream = NULL;
    int retval;

    if (!config)
    {
        return -RIG_ENOMEM;
    }

    config->type = type;
    config->format = (type == RIG_STREAM_TYPE_IQ_RX)
                     ? RIG_STREAM_FORMAT_IQ_CF32
                     : RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = sample_rate;
    config->channels = channels;
    config->require_native = g_require_native;

    printf("Opening %s stream: %d Hz, %d ch\n",
           type_name(type), sample_rate, channels);

    retval = rig_stream_open(rig, config, &stream);
    rig_stream_config_free(config);  /* stream kept its own copy */

    if (retval != RIG_OK)
    {
        fprintf(stderr, "rig_stream_open failed: %s\n", rigerror(retval));
        return retval;
    }

    report_conversions(stream, type_name(type));
    printf("Stream opened, reading...\n");

    if (iq_raw_path && type != RIG_STREAM_TYPE_IQ_RX)
    {
        fprintf(stderr, "-o/--iq-out applies only to iq_rx (use -w/--wav for audio)\n");
        rig_stream_close(rig, stream);
        return -RIG_EINVAL;
    }

    /* Open WAV file if requested */
    FILE *wav_fp = NULL;
    uint32_t wav_data_bytes = 0;
    FILE *iq_fp = NULL;

    if (iq_raw_path)
    {
        iq_fp = fopen(iq_raw_path, "wb");

        if (!iq_fp)
        {
            fprintf(stderr, "Cannot open IQ output file: %s\n", iq_raw_path);
            rig_stream_close(rig, stream);
            return -RIG_EIO;
        }

        printf("Writing IQ float32 (interleaved I,Q) to: %s\n", iq_raw_path);
    }

    if (wav_path)
    {
        wav_fp = fopen(wav_path, "wb");

        if (!wav_fp)
        {
            fprintf(stderr, "Cannot open WAV file: %s\n", wav_path);
            rig_stream_close(rig, stream);
            return -RIG_EIO;
        }

        wav_write_header(wav_fp, sample_rate, channels);
        printf("Writing WAV to: %s\n", wav_path);
    }

    /* Read buffer: 100ms worth of float32 samples. */
    int samples_per_100ms = (sample_rate * channels) / 10;
    size_t buf_bytes = samples_per_100ms * sizeof(float);
    float *buf = malloc(buf_bytes);

    if (!buf)
    {
        rig_stream_close(rig, stream);
        return -RIG_ENOMEM;
    }

    uint64_t total_bytes = 0;
    int elapsed = 0;
    time_t last_sec = time(NULL);
    int last_print = 0;

    while (running)
    {
        if (duration_sec > 0 && elapsed >= duration_sec)
        {
            break;
        }

        size_t bytes_read = 0;
        retval = rig_stream_read(rig, stream, buf, buf_bytes,
                                 &bytes_read, 500, NULL);

        if (retval == RIG_OK && bytes_read > 0)
        {
            total_bytes += bytes_read;

            if (iq_fp)
            {
                fwrite(buf, 1, bytes_read, iq_fp);
            }

            /* Convert float32 to int16 and write to WAV */
            if (wav_fp)
            {
                wav_append_f32(wav_fp, buf, bytes_read / sizeof(float),
                               &wav_data_bytes);
            }
        }
        else if (retval != RIG_OK && retval != -RIG_ETIMEOUT)
        {
            fprintf(stderr, "rig_stream_read error: %s\n", rigerror(retval));
            break;
        }

        time_t now = time(NULL);

        if (now != last_sec)
        {
            elapsed += (int)(now - last_sec);
            last_sec = now;

            if (elapsed > last_print)
            {
                last_print = elapsed;
                print_rx_stats(rig, stream, total_bytes, elapsed);

                if (bytes_read > 0)
                {
                    int num_samples = bytes_read / sizeof(float);
                    print_audio_level(buf, num_samples);
                }
            }
        }
    }

    printf("\nClosing stream...\n");
    print_rx_stats(rig, stream, total_bytes, elapsed);

    if (wav_fp)
    {
        wav_finalize(wav_fp, wav_data_bytes);
        fclose(wav_fp);
        printf("WAV file written: %u bytes audio data\n", wav_data_bytes);
    }

    if (iq_fp)
    {
        fclose(iq_fp);
    }

    free(buf);
    rig_stream_close(rig, stream);
    return RIG_OK;
}


/* Run TX: generate a test tone and write it to the stream. Audio and I/Q are
 * handled identically; use_iq selects the stream type, format and tone. */
static int run_tx_single(RIG *rig, int use_iq, int sample_rate, int channels,
                         int duration_sec, int do_ptt, int frame_ms)
{
    rig_stream_type_t type = use_iq ? RIG_STREAM_TYPE_IQ_TX :
                             RIG_STREAM_TYPE_AUDIO_TX;
    int chans = use_iq ? 1 : channels;
    int floats_per_frame = use_iq ? 2 : chans;   /* I/Q pair = 2 floats */

    struct rig_stream_config *config = rig_stream_config_alloc();
    rig_stream_t *stream = NULL;
    int retval;

    if (!config)
    {
        return -RIG_ENOMEM;
    }

    config->type = type;
    config->format = use_iq ? RIG_STREAM_FORMAT_IQ_CF32
                     : RIG_STREAM_FORMAT_PCM_F32;
    config->sample_rate = sample_rate;
    config->channels = chans;
    config->require_native = g_require_native;

    printf("Opening %s stream: %d Hz, %d ch%s\n",
           type_name(type), sample_rate, chans,
           use_iq ? " (interleaved I/Q)" : "");

    retval = rig_stream_open(rig, config, &stream);
    rig_stream_config_free(config);  /* stream kept its own copy */

    if (retval != RIG_OK)
    {
        fprintf(stderr, "rig_stream_open(%s) failed: %s\n",
                type_name(type), rigerror(retval));
        return retval;
    }

    report_conversions(stream, type_name(type));
    printf("Stream opened, generating 1 kHz tone (%d ms frames)...\n",
           frame_ms);

    /* frame_ms of samples per write, derived from the configured rate so no
     * rate or frame size is assumed anywhere. */
    int frame_pairs = sample_rate * frame_ms / 1000;

    if (frame_pairs < 1)
    {
        frame_pairs = 1;
    }

    size_t frame_bytes = (size_t)frame_pairs * floats_per_frame * sizeof(float);
    float *buf = malloc(frame_bytes);

    if (!buf)
    {
        rig_stream_close(rig, stream);
        return -RIG_ENOMEM;
    }

    uint64_t phase = 0;
    uint64_t total_bytes = 0;
    uint64_t frames_written = 0;
    int elapsed = 0;
    time_t last_sec = time(NULL);
    struct timespec pace_start;

    if (do_ptt)
    {
        ptt_t ptt = RIG_PTT_OFF;
        int pret = rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_ON);
        printf("PTT ON (transmitting into your load) -> %s\n",
               pret == RIG_OK ? "ok" : rigerror(pret));
        /* confirm the rig actually keyed by reading PTT back */
        pret = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);
        printf("PTT read-back: %s\n",
               pret != RIG_OK ? rigerror(pret)
               : ptt != RIG_PTT_OFF ? "ON (rig is keyed)"
               : "OFF -- RIG IS NOT KEYED!");
        fflush(stdout);
    }

    clock_gettime(CLOCK_MONOTONIC, &pace_start);

    while (running)
    {
        if (duration_sec > 0 && elapsed >= duration_sec)
        {
            break;
        }

        if (use_iq)
        {
            generate_iq_tone(buf, frame_pairs, sample_rate, &phase);
        }
        else
        {
            generate_tone(buf, frame_pairs, sample_rate, chans, &phase);
        }

        size_t bytes_written = 0;
        retval = rig_stream_write(rig, stream, buf, frame_bytes,
                                  &bytes_written, 500, NULL);

        if (retval == RIG_OK)
        {
            total_bytes += bytes_written;
            frames_written += bytes_written
                              / ((size_t)floats_per_frame * sizeof(float));
        }
        else if (retval != -RIG_ETIMEOUT)
        {
            fprintf(stderr, "rig_stream_write error: %s\n", rigerror(retval));
            break;
        }

        pace_to_frame(&pace_start, frames_written, sample_rate);

        time_t now = time(NULL);

        if (now != last_sec)
        {
            elapsed += (int)(now - last_sec);
            last_sec = now;
            printf("[%4ds] %s bytes=%llu  underruns=%u\n",
                   elapsed, type_name(type),
                   (unsigned long long)total_bytes,
                   stream_underruns(rig, stream));
            fflush(stdout);
        }
    }

    if (do_ptt)
    {
        int pret = rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);
        printf("PTT OFF -> %s\n", pret == RIG_OK ? "ok" : rigerror(pret));
        fflush(stdout);
    }

    printf("\nClosing %s stream (%llu bytes written)...\n",
           type_name(type), (unsigned long long)total_bytes);

    free(buf);
    rig_stream_close(rig, stream);
    return RIG_OK;
}


/* Run RX+TX loopback: read from audio RX, write to audio TX. */
static int run_loopback(RIG *rig, int sample_rate, int channels,
                        int duration_sec, const char *wav_path)
{
    struct rig_stream_config *rx_config = rig_stream_config_alloc();
    struct rig_stream_config *tx_config = rig_stream_config_alloc();
    rig_stream_t *rx_stream = NULL;
    rig_stream_t *tx_stream = NULL;
    int retval;

    if (!rx_config || !tx_config)
    {
        rig_stream_config_free(rx_config);
        rig_stream_config_free(tx_config);
        return -RIG_ENOMEM;
    }

    rx_config->type = RIG_STREAM_TYPE_AUDIO_RX;
    rx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    rx_config->sample_rate = sample_rate;
    rx_config->channels = channels;
    rx_config->require_native = g_require_native;

    tx_config->type = RIG_STREAM_TYPE_AUDIO_TX;
    tx_config->format = RIG_STREAM_FORMAT_PCM_F32;
    tx_config->sample_rate = sample_rate;
    tx_config->channels = channels;
    tx_config->require_native = g_require_native;

    printf("Opening loopback: AUDIO_RX -> AUDIO_TX, %d Hz, %d ch\n",
           sample_rate, channels);

    retval = rig_stream_open(rig, rx_config, &rx_stream);

    if (retval != RIG_OK)
    {
        fprintf(stderr, "rig_stream_open(RX) failed: %s\n", rigerror(retval));
        rig_stream_config_free(rx_config);
        rig_stream_config_free(tx_config);
        return retval;
    }

    retval = rig_stream_open(rig, tx_config, &tx_stream);
    /* Both streams kept their own copies; the configs are done. */
    rig_stream_config_free(rx_config);
    rig_stream_config_free(tx_config);

    if (retval != RIG_OK)
    {
        fprintf(stderr, "rig_stream_open(TX) failed: %s\n", rigerror(retval));
        rig_stream_close(rig, rx_stream);
        return retval;
    }

    report_conversions(rx_stream, "RX");
    report_conversions(tx_stream, "TX");
    printf("Loopback running...\n");

    /* Open WAV file if requested */
    FILE *wav_fp = NULL;
    uint32_t wav_data_bytes = 0;

    if (wav_path)
    {
        wav_fp = fopen(wav_path, "wb");

        if (!wav_fp)
        {
            fprintf(stderr, "Cannot open WAV file: %s\n", wav_path);
            rig_stream_close(rig, tx_stream);
            rig_stream_close(rig, rx_stream);
            return -RIG_EIO;
        }

        wav_write_header(wav_fp, sample_rate, channels);
        printf("Writing loopback RX to WAV: %s\n", wav_path);
    }

    size_t buf_bytes = 1024 * sizeof(float);
    float *buf = malloc(buf_bytes);

    if (!buf)
    {
        rig_stream_close(rig, tx_stream);
        rig_stream_close(rig, rx_stream);
        return -RIG_ENOMEM;
    }

    uint64_t total_rx = 0;
    uint64_t total_tx = 0;
    int elapsed = 0;
    time_t last_sec = time(NULL);

    while (running)
    {
        if (duration_sec > 0 && elapsed >= duration_sec)
        {
            break;
        }

        size_t bytes_read = 0;
        retval = rig_stream_read(rig, rx_stream, buf, buf_bytes,
                                 &bytes_read, 200, NULL);

        if (retval == RIG_OK && bytes_read > 0)
        {
            total_rx += bytes_read;

            /* Write RX audio to WAV */
            if (wav_fp)
            {
                wav_append_f32(wav_fp, buf, bytes_read / sizeof(float),
                               &wav_data_bytes);
            }

            size_t bytes_written = 0;
            retval = rig_stream_write(rig, tx_stream, buf, bytes_read,
                                      &bytes_written, 200, NULL);

            if (retval == RIG_OK)
            {
                total_tx += bytes_written;
            }
        }

        time_t now = time(NULL);

        if (now != last_sec)
        {
            elapsed += (int)(now - last_sec);
            last_sec = now;
            struct rig_stream_stats rx_st;
            memset(&rx_st, 0, sizeof(rx_st));
            rig_stream_get_stats(rig, rx_stream, &rx_st);
            printf("[%4ds] rx_bytes=%llu  tx_bytes=%llu  "
                   "gaps=%u  overruns=%u  underruns=%u\n",
                   elapsed,
                   (unsigned long long)total_rx,
                   (unsigned long long)total_tx,
                   rx_st.gaps, rx_st.overruns,
                   stream_underruns(rig, tx_stream));
            fflush(stdout);
        }
    }

    printf("\nClosing loopback (rx=%llu tx=%llu bytes)...\n",
           (unsigned long long)total_rx,
           (unsigned long long)total_tx);

    if (wav_fp)
    {
        wav_finalize(wav_fp, wav_data_bytes);
        fclose(wav_fp);
        printf("WAV file written: %u bytes audio data\n", wav_data_bytes);
    }

    free(buf);
    rig_stream_close(rig, tx_stream);
    rig_stream_close(rig, rx_stream);
    return RIG_OK;
}


/* --- Alternating RX/TX soak mode --- */

struct alt_opts
{
    int rx_secs;
    int tx_secs;
    int cycles;
    int stats_secs;
    int use_iq;
    int do_ptt;
    int set_power;
    float power;
    int sample_rate;
    int channels;
    int duration;   /* overall wall-clock cap, 0 = unbounded */
    int frame_ms;
    int buffer_ms;  /* ring buffer_duration_ms (0 = backend default) */
};

static void accumulate_stats(struct dir_stats *acc,
                             const struct rig_stream_stats *st)
{
    acc->gaps                 += st->gaps;
    acc->gaps_unknown         += st->gaps_unknown;
    acc->overruns             += st->overruns;
    acc->underruns            += st->underruns;
    acc->link_loss            += st->link_loss;
    acc->tx_late              += st->tx_late;
    acc->remote_overruns      += st->remote_overruns;
    acc->remote_underruns     += st->remote_underruns;
    acc->write_events_dropped += st->write_events_dropped;
    acc->dropped_gap          += st->dropped_samples_gap;
    acc->dropped_overrun      += st->dropped_samples_overrun;
    acc->dropped_link         += st->dropped_samples_link;
}

/* Open a stream, retrying once after a short pause to absorb a first-connect
 * transient. Returns RIG_OK and sets *stream on success. */
static int open_with_retry(RIG *rig, const struct rig_stream_config *cfg,
                           rig_stream_t **stream, struct err_tally *err)
{
    int ret = rig_stream_open(rig, cfg, stream);

    if (ret != RIG_OK && running)
    {
        fprintf(stderr, "  open failed (%s); retrying in 2s...\n",
                rigerror(ret));
        hl_usleep(2000000);
        ret = rig_stream_open(rig, cfg, stream);

        if (ret == RIG_OK)
        {
            err->open_retry++;
        }
    }

    if (ret != RIG_OK)
    {
        err->open_fail++;
    }
    else
    {
        report_conversions(*stream, "phase");
    }

    return ret;
}

/* One RX phase: drain for `secs` seconds (or until Ctrl-C), accumulate stats. */
static void run_rx_phase(RIG *rig, rig_stream_type_t type,
                         const struct alt_opts *o, int secs,
                         struct dir_stats *acc, struct err_tally *err)
{
    struct rig_stream_config *cfg = rig_stream_config_alloc();
    rig_stream_t *stream = NULL;

    if (!cfg)
    {
        acc->phase_failures++;
        return;
    }

    cfg->type = type;
    cfg->format = (type == RIG_STREAM_TYPE_IQ_RX)
                  ? RIG_STREAM_FORMAT_IQ_CF32 : RIG_STREAM_FORMAT_PCM_F32;
    cfg->sample_rate = o->sample_rate;
    cfg->channels = o->channels;
    cfg->require_native = g_require_native;
    cfg->buffer_duration_ms = o->buffer_ms;   /* 0 = backend default */

    int ret = open_with_retry(rig, cfg, &stream, err);
    rig_stream_config_free(cfg);

    if (ret != RIG_OK)
    {
        acc->phase_failures++;
        return;
    }

    printf("  [RX %s] phase (%d s)\n", type_name(type), secs);

    int per_100ms = (o->sample_rate * o->channels) / 10;

    if (per_100ms < 1)
    {
        per_100ms = 1;
    }

    size_t buf_bytes = (size_t)per_100ms * sizeof(float);
    float *buf = malloc(buf_bytes);

    if (!buf)
    {
        rig_stream_close(rig, stream);
        acc->phase_failures++;
        return;
    }

    int elapsed = 0, last_print = 0;
    uint64_t bytes_this_sec = 0;
    time_t last_sec = time(NULL);

    while (running && elapsed < secs)
    {
        size_t got = 0;
        ret = rig_stream_read(rig, stream, buf, buf_bytes, &got, 500, NULL);

        if (ret == RIG_OK && got > 0)
        {
            acc->bytes += got;
            bytes_this_sec += got;
        }
        else if (ret != RIG_OK && ret != -RIG_ETIMEOUT)
        {
            fprintf(stderr, "  rig_stream_read error: %s\n", rigerror(ret));
            err->read_err++;
            break;
        }

        time_t now = time(NULL);

        if (now != last_sec)
        {
            elapsed += (int)(now - last_sec);
            last_sec = now;

            if (bytes_this_sec == 0)
            {
                err->starvation++;   /* a full second with no RX data */
            }

            bytes_this_sec = 0;

            if (o->stats_secs > 0 && elapsed - last_print >= o->stats_secs)
            {
                last_print = elapsed;
                print_rx_stats(rig, stream, acc->bytes, elapsed);
            }
        }
    }

    struct rig_stream_stats st;

    memset(&st, 0, sizeof(st));
    rig_stream_get_stats(rig, stream, &st);
    accumulate_stats(acc, &st);
    acc->phases++;

    free(buf);
    rig_stream_close(rig, stream);
}

/* One TX phase: feed the sine wave for `secs` seconds, accumulate stats. */
static void run_tx_phase(RIG *rig, rig_stream_type_t type,
                         const struct alt_opts *o, int secs,
                         struct dir_stats *acc, struct err_tally *err)
{
    if (o->set_power)
    {
        value_t v;
        v.f = o->power;
        int pret = rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, v);

        if (pret != RIG_OK)
        {
            fprintf(stderr, "  rig_set_level(RFPOWER,%.2f) failed: %s\n",
                    o->power, rigerror(pret));
            err->power_fail++;
        }
    }

    int iq = (type == RIG_STREAM_TYPE_IQ_TX);
    int chans = iq ? 1 : o->channels;

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    rig_stream_t *stream = NULL;

    if (!cfg)
    {
        acc->phase_failures++;
        return;
    }

    cfg->type = type;
    cfg->format = iq ? RIG_STREAM_FORMAT_IQ_CF32 : RIG_STREAM_FORMAT_PCM_F32;
    cfg->sample_rate = o->sample_rate;
    cfg->channels = chans;
    cfg->require_native = g_require_native;
    cfg->buffer_duration_ms = o->buffer_ms;   /* 0 = backend default */

    int ret = open_with_retry(rig, cfg, &stream, err);
    rig_stream_config_free(cfg);

    if (ret != RIG_OK)
    {
        acc->phase_failures++;
        return;
    }

    printf("  [TX %s] phase (%d s)%s\n", type_name(type), secs,
           o->do_ptt ? " PTT ON (RF!)" : "");

    int frame_pairs = o->sample_rate * o->frame_ms / 1000;

    if (frame_pairs < 1)
    {
        frame_pairs = 1;
    }

    int floats_per_frame = iq ? 2 : chans;   /* I/Q pair = 2 floats */
    size_t frame_bytes = (size_t)frame_pairs * floats_per_frame * sizeof(float);
    float *buf = malloc(frame_bytes);

    if (!buf)
    {
        rig_stream_close(rig, stream);
        acc->phase_failures++;
        return;
    }

    if (o->do_ptt)
    {
        int pret = rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_ON);

        if (pret != RIG_OK)
        {
            fprintf(stderr, "  PTT ON failed: %s\n", rigerror(pret));
            err->ptt_fail++;
        }
    }

    uint64_t phase = 0, frames_written = 0;
    int elapsed = 0, last_print = 0;
    time_t last_sec = time(NULL);
    struct timespec pace_start;
    clock_gettime(CLOCK_MONOTONIC, &pace_start);

    while (running && elapsed < secs)
    {
        if (iq)
        {
            generate_iq_tone(buf, frame_pairs, o->sample_rate, &phase);
        }
        else
        {
            generate_tone(buf, frame_pairs, o->sample_rate, chans, &phase);
        }

        size_t wrote = 0;
        ret = rig_stream_write(rig, stream, buf, frame_bytes, &wrote, 500, NULL);

        if (ret == RIG_OK)
        {
            acc->bytes += wrote;
            frames_written += wrote
                              / ((size_t)floats_per_frame * sizeof(float));

            if (wrote < frame_bytes)
            {
                err->short_write++;
            }
        }
        else if (ret != -RIG_ETIMEOUT)
        {
            fprintf(stderr, "  rig_stream_write error: %s\n", rigerror(ret));
            err->write_err++;
            break;
        }

        pace_to_frame(&pace_start, frames_written, o->sample_rate);

        time_t now = time(NULL);

        if (now != last_sec)
        {
            elapsed += (int)(now - last_sec);
            last_sec = now;

            if (o->stats_secs > 0 && elapsed - last_print >= o->stats_secs)
            {
                last_print = elapsed;
                printf("  [TX %4ds] bytes=%llu  underruns=%u\n", elapsed,
                       (unsigned long long)acc->bytes,
                       stream_underruns(rig, stream));
                fflush(stdout);
            }
        }
    }

    if (o->do_ptt)
    {
        int pret = rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);

        if (pret != RIG_OK)
        {
            err->ptt_fail++;
        }
    }

    struct rig_stream_stats st;

    memset(&st, 0, sizeof(st));
    rig_stream_get_stats(rig, stream, &st);
    accumulate_stats(acc, &st);
    acc->phases++;

    free(buf);
    rig_stream_close(rig, stream);
}

/* Alternate RX and TX phases until --cycles / --duration / Ctrl-C, then print a
 * per-direction summary and a categorised issue tally. Returns 0 if no issues
 * were seen, 1 otherwise (so a soak wrapper can flag problems by exit code). */
static int run_alternating(RIG *rig, const struct alt_opts *o)
{
    struct dir_stats rx, tx;
    struct err_tally err;
    memset(&rx, 0, sizeof(rx));
    memset(&tx, 0, sizeof(tx));
    memset(&err, 0, sizeof(err));

    rig_stream_type_t rx_type = o->use_iq
                                ? RIG_STREAM_TYPE_IQ_RX : RIG_STREAM_TYPE_AUDIO_RX;
    rig_stream_type_t tx_type = o->use_iq
                                ? RIG_STREAM_TYPE_IQ_TX : RIG_STREAM_TYPE_AUDIO_TX;

    printf("Alternating soak: rx=%ds tx=%ds cycles=%d duration=%ds %s%s\n",
           o->rx_secs, o->tx_secs, o->cycles, o->duration,
           o->use_iq ? "IQ" : "audio", o->do_ptt ? " +PTT(RF!)" : "");

    time_t start = time(NULL);
    int cycle = 0;

    while (running)
    {
        if (o->duration > 0 && (int)(time(NULL) - start) >= o->duration)
        {
            break;
        }

        if (o->cycles > 0 && cycle >= o->cycles)
        {
            break;
        }

        if (o->rx_secs > 0 && running)
        {
            run_rx_phase(rig, rx_type, o, o->rx_secs, &rx, &err);
        }

        if (o->tx_secs > 0 && running)
        {
            run_tx_phase(rig, tx_type, o, o->tx_secs, &tx, &err);
        }

        cycle++;
        printf("--- cycle %d: RX bytes=%llu(fail %llu)  TX bytes=%llu(fail %llu)\n",
               cycle,
               (unsigned long long)rx.bytes, (unsigned long long)rx.phase_failures,
               (unsigned long long)tx.bytes, (unsigned long long)tx.phase_failures);
        fflush(stdout);
    }

    printf("\n===== soak summary: %d cycles =====\n", cycle);
    printf("RX: phases=%llu fail=%llu bytes=%llu gaps=%llu(%llu uns) "
           "ovr=%llu und=%llu link=%llu dropped(g/o/l)=%llu/%llu/%llu\n",
           (unsigned long long)rx.phases, (unsigned long long)rx.phase_failures,
           (unsigned long long)rx.bytes, (unsigned long long)rx.gaps,
           (unsigned long long)rx.gaps_unknown, (unsigned long long)rx.overruns,
           (unsigned long long)rx.underruns, (unsigned long long)rx.link_loss,
           (unsigned long long)rx.dropped_gap, (unsigned long long)rx.dropped_overrun,
           (unsigned long long)rx.dropped_link);
    printf("TX: phases=%llu fail=%llu bytes=%llu gaps=%llu ovr=%llu "
           "und=%llu link=%llu tx_late=%llu "
           "rem(ovr/und)=%llu/%llu write_events_dropped=%llu\n",
           (unsigned long long)tx.phases, (unsigned long long)tx.phase_failures,
           (unsigned long long)tx.bytes, (unsigned long long)tx.gaps,
           (unsigned long long)tx.overruns, (unsigned long long)tx.underruns,
           (unsigned long long)tx.link_loss, (unsigned long long)tx.tx_late,
           (unsigned long long)tx.remote_overruns,
           (unsigned long long)tx.remote_underruns,
           (unsigned long long)tx.write_events_dropped);
    printf("issues: open_fail=%llu open_retry=%llu read_err=%llu write_err=%llu "
           "ptt_fail=%llu power_fail=%llu starvation=%llu short_write=%llu\n",
           (unsigned long long)err.open_fail, (unsigned long long)err.open_retry,
           (unsigned long long)err.read_err, (unsigned long long)err.write_err,
           (unsigned long long)err.ptt_fail, (unsigned long long)err.power_fail,
           (unsigned long long)err.starvation, (unsigned long long)err.short_write);

    uint64_t issues = total_issues(&rx, &tx, &err);
    printf("TOTAL ISSUES: %llu\n", (unsigned long long)issues);
    fflush(stdout);
    return issues ? 1 : 0;
}


/* --- Full-duplex mode: RX and TX streams concurrent, PTT cycled --- */

struct duplex_opts
{
    int use_iq;
    int do_ptt;
    int tx_secs;        /* PTT ON seconds (0 = never key) */
    int rx_secs;        /* PTT OFF seconds (0 = hold key on when tx_secs > 0) */
    int tone_on_ms;     /* 0/0 = tone always on */
    int tone_off_ms;
    int set_power;
    float power;
    int sample_rate;
    int channels;
    int duration;
    int frame_ms;
    int buffer_ms;
    int stats_secs;
};

/* Milliseconds elapsed since a monotonic start. */
static long ms_since(const struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long)((now.tv_sec - start->tv_sec) * 1000L
                  + (now.tv_nsec - start->tv_nsec) / 1000000L);
}

/* RX worker: read continuously into a discard buffer and track bytes/errors.
 * A read timeout is normal here (the radio produces no RX audio while PTT is
 * keyed), so only genuine errors are counted. */
struct fd_rx_arg
{
    RIG *rig;
    rig_stream_t *stream;
    int sample_rate;
    int channels;
    uint64_t bytes;         /* thread-local; read loosely by the controller */
    struct err_tally err;   /* thread-local; merged after join */
};

static void *duplex_rx_thread(void *arg)
{
    struct fd_rx_arg *a = arg;
    int per_100ms = (a->sample_rate * a->channels) / 10;

    if (per_100ms < 1)
    {
        per_100ms = 1;
    }

    size_t buf_bytes = (size_t)per_100ms * sizeof(float);
    float *buf = malloc(buf_bytes);

    if (!buf)
    {
        a->err.read_err++;
        return NULL;
    }

    while (running)
    {
        size_t got = 0;
        int ret = rig_stream_read(a->rig, a->stream, buf, buf_bytes,
                                  &got, 500, NULL);

        if (ret == RIG_OK && got > 0)
        {
            a->bytes += got;
        }
        else if (ret != RIG_OK && ret != -RIG_ETIMEOUT)
        {
            a->err.read_err++;
        }
    }

    free(buf);
    return NULL;
}

/* TX worker: write the tone (or silence, per the free-running on/off schedule)
 * continuously, paced to real time. PTT is handled by the controller. */
struct fd_tx_arg
{
    RIG *rig;
    rig_stream_t *stream;
    int sample_rate;
    int channels;
    int frame_ms;
    int iq;
    int tone_on_ms;
    int tone_off_ms;
    uint64_t bytes;         /* thread-local; read loosely by the controller */
    struct err_tally err;
};

static void *duplex_tx_thread(void *arg)
{
    struct fd_tx_arg *a = arg;
    int floats_per_frame = a->iq ? 2 : a->channels;   /* I/Q pair = 2 floats */
    int frame_pairs = a->sample_rate * a->frame_ms / 1000;

    if (frame_pairs < 1)
    {
        frame_pairs = 1;
    }

    size_t frame_bytes = (size_t)frame_pairs * floats_per_frame * sizeof(float);
    float *buf = malloc(frame_bytes);

    if (!buf)
    {
        a->err.write_err++;
        return NULL;
    }

    uint64_t phase = 0, frames_written = 0;
    long tone_period = a->tone_on_ms + a->tone_off_ms;
    struct timespec pace_start, tone_start;
    clock_gettime(CLOCK_MONOTONIC, &pace_start);
    tone_start = pace_start;

    while (running)
    {
        int play = 1;

        if (tone_period > 0)
        {
            long pos = ms_since(&tone_start) % tone_period;
            play = (pos < a->tone_on_ms);
        }

        if (play)
        {
            if (a->iq)
            {
                generate_iq_tone(buf, frame_pairs, a->sample_rate, &phase);
            }
            else
            {
                generate_tone(buf, frame_pairs, a->sample_rate, a->channels,
                              &phase);
            }
        }
        else
        {
            memset(buf, 0, frame_bytes);
            phase += frame_pairs;   /* keep tone phase continuous across silence */
        }

        size_t wrote = 0;
        int ret = rig_stream_write(a->rig, a->stream, buf, frame_bytes,
                                   &wrote, 500, NULL);

        if (ret == RIG_OK)
        {
            a->bytes += wrote;
            frames_written += wrote / ((size_t)floats_per_frame * sizeof(float));

            if (wrote < frame_bytes)
            {
                a->err.short_write++;
            }
        }
        else if (ret != -RIG_ETIMEOUT)
        {
            a->err.write_err++;
        }

        pace_to_frame(&pace_start, frames_written, a->sample_rate);
    }

    free(buf);
    return NULL;
}

/* One interim line showing BOTH directions' live stats plus PTT/tone state. */
static void print_duplex_stats(RIG *rig, rig_stream_t *rx, rig_stream_t *tx,
                               int elapsed, uint64_t rx_bytes, uint64_t tx_bytes,
                               const char *ptt, const char *tone)
{
    struct rig_stream_stats rs, ts;
    memset(&rs, 0, sizeof(rs));
    memset(&ts, 0, sizeof(ts));
    rig_stream_get_stats(rig, rx, &rs);
    rig_stream_get_stats(rig, tx, &ts);

    printf("[%4ds] PTT=%s tone=%s | RX bytes=%llu gaps=%u ovr=%u und=%u "
           "link=%u | TX bytes=%llu tx_late=%u und=%u ovr=%u\n",
           elapsed, ptt, tone,
           (unsigned long long)rx_bytes, rs.gaps, rs.overruns, rs.underruns,
           rs.link_loss,
           (unsigned long long)tx_bytes, ts.tx_late, ts.underruns, ts.overruns);
    fflush(stdout);
}

/* Open one RX and one TX stream, run them concurrently for the whole run while
 * cycling PTT per --tx-secs/--rx-secs, and report both directions. Returns 0
 * if no issues were tallied, 1 otherwise. */
static int run_full_duplex(RIG *rig, const struct duplex_opts *o)
{
    rig_stream_type_t rx_type = o->use_iq
                                ? RIG_STREAM_TYPE_IQ_RX : RIG_STREAM_TYPE_AUDIO_RX;
    rig_stream_type_t tx_type = o->use_iq
                                ? RIG_STREAM_TYPE_IQ_TX : RIG_STREAM_TYPE_AUDIO_TX;
    int tx_chans = o->use_iq ? 1 : o->channels;
    struct err_tally cerr;
    memset(&cerr, 0, sizeof(cerr));

    printf("Full-duplex: tx(PTT-on)=%ds rx(PTT-off)=%ds tone=%d/%d ms "
           "duration=%ds %s%s\n",
           o->tx_secs, o->rx_secs, o->tone_on_ms, o->tone_off_ms, o->duration,
           o->use_iq ? "IQ" : "audio", o->do_ptt ? " +PTT(RF!)" : " (no PTT)");

    if (o->set_power)
    {
        value_t v;
        v.f = o->power;
        int pret = rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, v);

        if (pret != RIG_OK)
        {
            fprintf(stderr, "  rig_set_level(RFPOWER,%.2f) failed: %s\n",
                    o->power, rigerror(pret));
            cerr.power_fail++;
        }
    }

    /* Open both streams for the whole run. */
    struct rig_stream_config *rx_cfg = rig_stream_config_alloc();
    struct rig_stream_config *tx_cfg = rig_stream_config_alloc();
    rig_stream_t *rx = NULL, *tx = NULL;

    if (!rx_cfg || !tx_cfg)
    {
        rig_stream_config_free(rx_cfg);
        rig_stream_config_free(tx_cfg);
        return 1;
    }

    rx_cfg->type = rx_type;
    rx_cfg->format = o->use_iq ? RIG_STREAM_FORMAT_IQ_CF32 :
                     RIG_STREAM_FORMAT_PCM_F32;
    rx_cfg->sample_rate = o->sample_rate;
    rx_cfg->channels = o->channels;
    rx_cfg->require_native = g_require_native;
    rx_cfg->buffer_duration_ms = o->buffer_ms;

    tx_cfg->type = tx_type;
    tx_cfg->format = o->use_iq ? RIG_STREAM_FORMAT_IQ_CF32 :
                     RIG_STREAM_FORMAT_PCM_F32;
    tx_cfg->sample_rate = o->sample_rate;
    tx_cfg->channels = tx_chans;
    tx_cfg->require_native = g_require_native;
    tx_cfg->buffer_duration_ms = o->buffer_ms;

    int rx_ret = rig_stream_open(rig, rx_cfg, &rx);
    int tx_ret = rig_stream_open(rig, tx_cfg, &tx);
    rig_stream_config_free(rx_cfg);
    rig_stream_config_free(tx_cfg);

    if (rx_ret != RIG_OK || tx_ret != RIG_OK)
    {
        fprintf(stderr, "  full-duplex open failed: RX=%s TX=%s\n",
                rigerror(rx_ret), rigerror(tx_ret));

        if (rx) { rig_stream_close(rig, rx); }

        if (tx) { rig_stream_close(rig, tx); }

        return 1;
    }

    report_conversions(rx, "RX");
    report_conversions(tx, "TX");

    struct fd_rx_arg rxa;

    struct fd_tx_arg txa;
    memset(&rxa, 0, sizeof(rxa));
    memset(&txa, 0, sizeof(txa));
    rxa.rig = rig; rxa.stream = rx;
    rxa.sample_rate = o->sample_rate; rxa.channels = o->channels;
    txa.rig = rig; txa.stream = tx;
    txa.sample_rate = o->sample_rate; txa.channels = tx_chans;
    txa.frame_ms = o->frame_ms; txa.iq = o->use_iq;
    txa.tone_on_ms = o->tone_on_ms; txa.tone_off_ms = o->tone_off_ms;

    pthread_t rxth, txth;
    pthread_create(&rxth, NULL, duplex_rx_thread, &rxa);
    pthread_create(&txth, NULL, duplex_tx_thread, &txa);

    /* Controller: PTT cycling + interim combined stats + duration/Ctrl-C. */
    time_t start = time(NULL);
    struct timespec tone_ref;
    clock_gettime(CLOCK_MONOTONIC, &tone_ref);
    long tone_period = o->tone_on_ms + o->tone_off_ms;
    int last_print = 0;
    int ptt_on = 0;
    int keyed = 0;   /* have we ever keyed (so teardown unkeys) */

    while (running)
    {
        int elapsed = (int)(time(NULL) - start);

        if (o->duration > 0 && elapsed >= o->duration)
        {
            break;
        }

        /* Desired PTT state. */
        int want = 0;

        if (o->do_ptt && o->tx_secs > 0)
        {
            if (o->rx_secs == 0)
            {
                want = 1;   /* hold key on for the whole run */
            }
            else
            {
                int pos = elapsed % (o->tx_secs + o->rx_secs);
                want = (pos < o->tx_secs) ? 1 : 0;
            }
        }

        if (o->do_ptt && want != ptt_on)
        {
            int pr = rig_set_ptt(rig, RIG_VFO_CURR,
                                 want ? RIG_PTT_ON : RIG_PTT_OFF);

            if (pr != RIG_OK)
            {
                cerr.ptt_fail++;
            }

            ptt_on = want;
            keyed = keyed || want;
        }

        if (o->stats_secs > 0 && elapsed - last_print >= o->stats_secs)
        {
            last_print = elapsed;
            const char *tone = "on";

            if (tone_period > 0)
            {
                tone = (ms_since(&tone_ref) % tone_period < o->tone_on_ms)
                       ? "on" : "off";
            }

            print_duplex_stats(rig, rx, tx, elapsed, rxa.bytes, txa.bytes,
                               o->do_ptt ? (ptt_on ? "ON" : "off") : "n/a", tone);
        }

        hl_usleep(200000);   /* 200 ms controller tick */
    }

    running = 0;   /* stop the workers */
    pthread_join(rxth, NULL);
    pthread_join(txth, NULL);

    if (o->do_ptt && keyed)
    {
        rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);
    }

    /* Merge the disjoint per-thread issue counters. */
    struct err_tally err;
    memset(&err, 0, sizeof(err));
    err.read_err   = rxa.err.read_err;
    err.write_err  = txa.err.write_err;
    err.short_write = txa.err.short_write;
    err.ptt_fail   = cerr.ptt_fail;
    err.power_fail = cerr.power_fail;

    struct rig_stream_stats rs, ts;
    memset(&rs, 0, sizeof(rs));
    memset(&ts, 0, sizeof(ts));
    rig_stream_get_stats(rig, rx, &rs);
    rig_stream_get_stats(rig, tx, &ts);

    printf("\n===== full-duplex summary =====\n");
    printf("RX: bytes=%llu gaps=%llu(%llu uns) ovr=%llu und=%llu link=%llu "
           "dropped(g/o/l)=%llu/%llu/%llu\n",
           (unsigned long long)rxa.bytes,
           (unsigned long long)rs.gaps, (unsigned long long)rs.gaps_unknown,
           (unsigned long long)rs.overruns, (unsigned long long)rs.underruns,
           (unsigned long long)rs.link_loss,
           (unsigned long long)rs.dropped_samples_gap,
           (unsigned long long)rs.dropped_samples_overrun,
           (unsigned long long)rs.dropped_samples_link);
    printf("TX: bytes=%llu gaps=%llu tx_late=%llu und=%llu ovr=%llu "
           "link=%llu rem(ovr/und)=%llu/%llu write_events_dropped=%llu\n",
           (unsigned long long)txa.bytes, (unsigned long long)ts.gaps,
           (unsigned long long)ts.tx_late, (unsigned long long)ts.underruns,
           (unsigned long long)ts.overruns, (unsigned long long)ts.link_loss,
           (unsigned long long)ts.remote_overruns,
           (unsigned long long)ts.remote_underruns,
           (unsigned long long)ts.write_events_dropped);
    printf("issues: read_err=%llu write_err=%llu short_write=%llu "
           "ptt_fail=%llu power_fail=%llu\n",
           (unsigned long long)err.read_err, (unsigned long long)err.write_err,
           (unsigned long long)err.short_write,
           (unsigned long long)err.ptt_fail, (unsigned long long)err.power_fail);

    /* Fold the final RX/TX stream stats (gaps, over/underruns, link loss, ...)
     * into the issue tally so any lost data or missed deadline sets the exit
     * code. */
    struct dir_stats rxds, txds;
    memset(&rxds, 0, sizeof(rxds));
    memset(&txds, 0, sizeof(txds));
    accumulate_stats(&rxds, &rs);
    accumulate_stats(&txds, &ts);
    uint64_t issues = total_issues(&rxds, &txds, &err);
    printf("TOTAL ISSUES: %llu\n", (unsigned long long)issues);
    fflush(stdout);

    rig_stream_close(rig, rx);
    rig_stream_close(rig, tx);
    return issues ? 1 : 0;
}


int main(int argc, char *argv[])
{
    rig_model_t model = RIG_MODEL_NONE;
    const char *rig_file = NULL;
    const char *stream_type = "audio_rx";
    const char *wav_path = NULL;
    const char *iq_out_path = NULL;
    int sample_rate = 24000;
    int channels = 2;
    int duration = 0;
    int verbose = 0;
    int do_ptt = 0;
    int frame_ms = 20;
    const char *set_conf_str = NULL;

    /* Alternating-soak and full-duplex options (long-only). */
    int rx_secs = 0, tx_secs = 0, cycles = 0, stats_secs = 5;
    int use_iq = 0, set_power = 0, buffer_ms = 0;
    int full_duplex = 0, tone_on_ms = 0, tone_off_ms = 0;
    float power = 0.0f;
    enum
    {
        OPT_RX_SECS = 1000, OPT_TX_SECS, OPT_CYCLES,
        OPT_POWER, OPT_IQ, OPT_STATS_SECS, OPT_BUFFER_MS,
        OPT_FULL_DUPLEX, OPT_TONE_ON, OPT_TONE_OFF,
        OPT_REQUIRE_NATIVE
    };

    static struct option long_opts[] =
    {
        { "model",       required_argument, NULL, 'm' },
        { "rig-file",    required_argument, NULL, 'r' },
        { "type",        required_argument, NULL, 't' },
        { "sample-rate", required_argument, NULL, 's' },
        { "channels",    required_argument, NULL, 'c' },
        { "set-conf",    required_argument, NULL, 'C' },
        { "duration",    required_argument, NULL, 'd' },
        { "frame-ms",    required_argument, NULL, 'F' },
        { "wav",         required_argument, NULL, 'w' },
        { "iq-out",         required_argument, NULL, 'o' },
        { "ptt",         no_argument,       NULL, 'P' },
        { "verbose",     no_argument,       NULL, 'v' },
        { "help",        no_argument,       NULL, 'h' },
        { "rx-secs",     required_argument, NULL, OPT_RX_SECS },
        { "tx-secs",     required_argument, NULL, OPT_TX_SECS },
        { "cycles",      required_argument, NULL, OPT_CYCLES },
        { "power",       required_argument, NULL, OPT_POWER },
        { "iq",          no_argument,       NULL, OPT_IQ },
        { "stats-secs",  required_argument, NULL, OPT_STATS_SECS },
        { "buffer-ms",   required_argument, NULL, OPT_BUFFER_MS },
        { "require-native", optional_argument, NULL, OPT_REQUIRE_NATIVE },
        { "full-duplex", no_argument,       NULL, OPT_FULL_DUPLEX },
        { "tone-on-ms",     required_argument, NULL, OPT_TONE_ON },
        { "tone-off-ms",    required_argument, NULL, OPT_TONE_OFF },
        { NULL, 0, NULL, 0 }
    };

    int opt;

    while ((opt = getopt_long(argc, argv, "m:r:t:s:c:C:d:F:w:o:Pvh", long_opts,
                              NULL)) != -1)
    {
        switch (opt)
        {
        case 'm':
            model = atoi(optarg);
            break;

        case 'r':
            rig_file = optarg;
            break;

        case 't':
            stream_type = optarg;
            break;

        case 's':
            sample_rate = atoi(optarg);
            break;

        case 'c':
            channels = atoi(optarg);
            break;

        case 'C':
            set_conf_str = optarg;
            break;

        case 'd':
            duration = atoi(optarg);
            break;

        case 'F':
            frame_ms = atoi(optarg);

            if (frame_ms < 1 || frame_ms > 500)
            {
                fprintf(stderr, "Error: --frame-ms must be 1..500\n");
                return 1;
            }

            break;

        case 'w':
            wav_path = optarg;
            break;

        case 'o':
            iq_out_path = optarg;
            break;

        case 'P':
            do_ptt = 1;
            break;

        case 'v':
            verbose++;
            break;

        case OPT_RX_SECS:
            rx_secs = atoi(optarg);
            break;

        case OPT_TX_SECS:
            tx_secs = atoi(optarg);
            break;

        case OPT_CYCLES:
            cycles = atoi(optarg);
            break;

        case OPT_POWER:
            power = (float)atof(optarg);
            set_power = 1;

            if (power < 0.0f || power > 1.0f)
            {
                fprintf(stderr, "Error: --power must be 0.0..1.0\n");
                return 1;
            }

            break;

        case OPT_IQ:
            use_iq = 1;
            break;

        case OPT_STATS_SECS:
            stats_secs = atoi(optarg);
            break;

        case OPT_BUFFER_MS:
            buffer_ms = atoi(optarg);
            break;

        case OPT_REQUIRE_NATIVE:

            /* Bare flag = every stage, i.e. a fully native stream. */
            if (!optarg)
            {
                g_require_native = RIG_STREAM_CONV_ALL;
            }
            else if (stream_native_req_parse(optarg, &g_require_native) < 0)
            {
                fprintf(stderr,
                        "Error: --require-native: unknown stage in '%s' "
                        "(expected FORMAT, RATE, CHANNELS, ALL or NONE)\n",
                        optarg);
                return 1;
            }

            break;

        case OPT_FULL_DUPLEX:
            full_duplex = 1;
            break;

        case OPT_TONE_ON:
            tone_on_ms = atoi(optarg);
            break;

        case OPT_TONE_OFF:
            tone_off_ms = atoi(optarg);
            break;

        case 'h':
        default:
            usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }

    if (model == RIG_MODEL_NONE)
    {
        fprintf(stderr, "Error: --model is required\n");
        usage(argv[0]);
        return 1;
    }

    /* Set debug level based on verbosity. */
    rig_set_debug(verbose == 0 ? RIG_DEBUG_WARN :
                  verbose == 1 ? RIG_DEBUG_VERBOSE :
                  RIG_DEBUG_TRACE);

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    RIG *rig = rig_init(model);

    if (!rig)
    {
        fprintf(stderr, "Error: rig_init(%d) failed — unknown model?\n", model);
        return 2;
    }

    if (rig_file)
    {
        rig_set_conf(rig, TOK_PATHNAME, rig_file);
    }

    if (set_conf_str)
    {
        char conf_copy[1024];
        char *tok, *saveptr = NULL;
        strncpy(conf_copy, set_conf_str, sizeof(conf_copy) - 1);
        conf_copy[sizeof(conf_copy) - 1] = '\0';

        for (tok = strtok_r(conf_copy, ",", &saveptr); tok != NULL;
                tok = strtok_r(NULL, ",", &saveptr))
        {
            char *eq = strchr(tok, '=');

            if (eq)
            {
                *eq = '\0';
                rig_set_conf(rig, rig_token_lookup(rig, tok), eq + 1);
            }
        }
    }

    printf("Opening rig model %d", model);

    if (rig_file)
    {
        printf(" at %s", rig_file);
    }

    printf("...\n");

    int retval = rig_open(rig);

    if (retval != RIG_OK)
    {
        fprintf(stderr, "rig_open failed: %s\n", rigerror(retval));
        rig_cleanup(rig);
        return 2;
    }

    printf("Rig opened: %s\n", rig->caps->model_name);

    /* Full-duplex mode: both streams concurrent, PTT cycled. Overrides the
     * alternating and single-shot dispatches. */
    if (full_duplex)
    {
        struct duplex_opts fo;
        memset(&fo, 0, sizeof(fo));
        fo.use_iq = use_iq;
        fo.do_ptt = do_ptt;
        fo.tx_secs = tx_secs;
        fo.rx_secs = rx_secs;
        fo.tone_on_ms = tone_on_ms;
        fo.tone_off_ms = tone_off_ms;
        fo.set_power = set_power;
        fo.power = power;
        fo.sample_rate = sample_rate;
        fo.channels = channels;
        fo.duration = duration;
        fo.frame_ms = frame_ms;
        fo.buffer_ms = buffer_ms;
        fo.stats_secs = stats_secs;

        int fd_rc = run_full_duplex(rig, &fo);
        retval = fd_rc ? -RIG_EIO : RIG_OK;
    }
    /* Alternating RX/TX soak mode: active when a phase duration is given.
     * Overrides the single-shot -t dispatch below. */
    else if (rx_secs > 0 || tx_secs > 0)
    {
        struct alt_opts ao;
        memset(&ao, 0, sizeof(ao));
        ao.rx_secs = rx_secs;
        ao.tx_secs = tx_secs;
        ao.cycles = cycles;
        ao.stats_secs = stats_secs;
        ao.use_iq = use_iq;
        ao.do_ptt = do_ptt;
        ao.set_power = set_power;
        ao.power = power;
        ao.sample_rate = sample_rate;
        ao.channels = channels;
        ao.duration = duration;
        ao.frame_ms = frame_ms;
        ao.buffer_ms = buffer_ms;

        int alt_rc = run_alternating(rig, &ao);
        retval = alt_rc ? -RIG_EIO : RIG_OK;
    }
    /* Dispatch to the requested stream test mode. */
    else if (strcmp(stream_type, "audio_rx") == 0)
    {
        retval = run_rx_single(rig, RIG_STREAM_TYPE_AUDIO_RX,
                               sample_rate, channels, duration, wav_path, NULL);
    }
    else if (strcmp(stream_type, "audio_tx") == 0)
    {
        retval = run_tx_single(rig, 0, sample_rate, channels, duration,
                               do_ptt, frame_ms);
    }
    else if (strcmp(stream_type, "iq_rx") == 0)
    {
        retval = run_rx_single(rig, RIG_STREAM_TYPE_IQ_RX,
                               sample_rate, channels, duration, wav_path,
                               iq_out_path);
    }
    else if (strcmp(stream_type, "iq_tx") == 0)
    {
        retval = run_tx_single(rig, 1, sample_rate, channels, duration,
                               do_ptt, frame_ms);
    }
    else if (strcmp(stream_type, "loopback") == 0)
    {
        retval = run_loopback(rig, sample_rate, channels, duration, wav_path);
    }
    else
    {
        fprintf(stderr, "Unknown stream type: %s\n", stream_type);
        fprintf(stderr, "Valid types: audio_rx, audio_tx, iq_rx, iq_tx, loopback\n");
        retval = -RIG_EINVAL;
    }

    printf("Closing rig...\n");
    rig_close(rig);
    rig_cleanup(rig);

    printf("Done (result=%d).\n", retval);
    return (retval == RIG_OK) ? 0 : 1;
}
