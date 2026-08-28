/*
 *  Hamlib streaming test tool helpers
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

/* Hardware-independent helpers for the rigstreamtest tool: test-tone
 * synthesis, WAV file output, and issue-tally accounting. */
/* Kept apart from the live-rig run loops so unit tests can exercise them. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "rigstreamtest_util.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


uint64_t dir_issues(const struct dir_stats *d)
{
    return d->phase_failures
           + d->gaps + d->overruns + d->underruns + d->link_loss
           + d->tx_late + d->remote_overruns + d->remote_underruns
           + d->write_events_dropped;
}


uint64_t total_issues(const struct dir_stats *rx,
                      const struct dir_stats *tx,
                      const struct err_tally *e)
{
    return dir_issues(rx) + dir_issues(tx)
           + e->open_fail + e->read_err + e->write_err
           + e->ptt_fail + e->power_fail + e->starvation + e->short_write;
}


void generate_tone(float *buf, int frames, int sample_rate,
                   int channels, uint64_t *phase_counter)
{
    double freq = 1000.0;

    for (int i = 0; i < frames; i++)
    {
        float val = 0.5f * sinf(2.0f * M_PI * freq
                                * (*phase_counter) / sample_rate);
        int c;

        for (c = 0; c < channels; c++)
        {
            buf[i * channels + c] = val;
        }

        (*phase_counter)++;
    }
}


void generate_iq_tone(float *buf, int iq_pairs, int sample_rate,
                      uint64_t *phase_counter)
{
    const float freq_hz = 1000.0f;

    for (int i = 0; i < iq_pairs; i++)
    {
        float ang = 2.0f * M_PI * freq_hz * (*phase_counter) / sample_rate;
        buf[i * 2] = 0.1f * sinf(ang);
        buf[i * 2 + 1] = 0.1f * cosf(ang);
        (*phase_counter)++;
    }
}


int wav_write_header(FILE *fp, int sample_rate, int channels)
{
    uint32_t data_size = 0;  /* placeholder, updated at close */
    uint16_t bits = 16;
    uint16_t block_align = channels * (bits / 8);
    uint32_t byte_rate = sample_rate * block_align;
    uint32_t riff_size = 36 + data_size;

    fwrite("RIFF", 1, 4, fp);
    fwrite(&riff_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);

    uint32_t fmt_size = 16;
    uint16_t audio_fmt = 1;  /* PCM */
    uint16_t num_ch = channels;
    uint32_t sr = sample_rate;

    fwrite(&fmt_size, 4, 1, fp);
    fwrite(&audio_fmt, 2, 1, fp);
    fwrite(&num_ch, 2, 1, fp);
    fwrite(&sr, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits, 2, 1, fp);

    fwrite("data", 1, 4, fp);
    fwrite(&data_size, 4, 1, fp);

    return 0;
}


void wav_finalize(FILE *fp, uint32_t data_bytes)
{
    uint32_t riff_size = 36 + data_bytes;

    fseek(fp, 4, SEEK_SET);
    fwrite(&riff_size, 4, 1, fp);

    fseek(fp, 40, SEEK_SET);
    fwrite(&data_bytes, 4, 1, fp);
}


void wav_append_f32(FILE *fp, const float *buf, int num_samples,
                    uint32_t *data_bytes)
{
    for (int i = 0; i < num_samples; i++)
    {
        float f = buf[i];

        if (f > 1.0f) { f = 1.0f; }

        if (f < -1.0f) { f = -1.0f; }

        int16_t s = (int16_t)(f * 32767.0f);
        fwrite(&s, sizeof(int16_t), 1, fp);
        *data_bytes += sizeof(int16_t);
    }
}
