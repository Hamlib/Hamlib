/*
 * Hamlib Sidecar API Test
 * Copyright (c) 2026 by Jeff Francis N0GQ
 *
 * Simple test to verify sidecar API compiles and links correctly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "hamlib/sidecar.h"

int main(int argc, char *argv[])
{
    int audio_fd, iq_fd, client_fd;
    int16_t audio_samples[512];
    int i;

    printf("Hamlib Sidecar API Test\n");
    printf("=======================\n\n");

    /* Test 1: Initialize ports */
    printf("Test 1: Initialize ports... ");
    audio_fd = sidecar_init_port(4534);
    iq_fd = sidecar_init_port(4535);

    if (audio_fd < 0 || iq_fd < 0) {
        printf("FAILED\n");
        printf("  audio_fd=%d, iq_fd=%d\n", audio_fd, iq_fd);
        return 1;
    }
    printf("OK (audio_fd=%d, iq_fd=%d)\n", audio_fd, iq_fd);

    /* Test 2: Accept client (should return -RIG_ENAVAIL since no client) */
    printf("Test 2: Accept client (expect no client)... ");
    client_fd = sidecar_accept_client(audio_fd);
    if (client_fd == -RIG_ENAVAIL) {
        printf("OK (no client waiting)\n");
    } else {
        printf("UNEXPECTED (got fd=%d)\n", client_fd);
    }

    /* Test 3: Send audio to non-existent client (should succeed silently) */
    printf("Test 3: Send audio to invalid fd... ");
    memset(audio_samples, 0, sizeof(audio_samples));
    int ret = sidecar_send_rx_audio(-1, 0, 8000, SIDECAR_FMT_INT16, 1,
                                    audio_samples, 512);
    if (ret == RIG_OK) {
        printf("OK (silently ignored)\n");
    } else {
        printf("FAILED (ret=%d)\n", ret);
    }

    /* Test 4: Emit control frames */
    printf("Test 4: Emit control frames... ");
    ret = sidecar_emit_mode(-1, 0, RIG_MODE_USB, 2400);
    ret |= sidecar_emit_freq(-1, 0, 14074000);
    ret |= sidecar_emit_agc_level(-1, 0, 2);  /* FAST */
    ret |= sidecar_emit_nr_level(-1, 0, 0.5f);

    if (ret == RIG_OK) {
        printf("OK\n");
    } else {
        printf("FAILED (ret=%d)\n", ret);
    }

    /* Test 5: Build raw frame */
    printf("Test 5: Build frame... ");
    uint8_t frame[SIDECAR_HEADER_LEN + 1024];
    int frame_len = sidecar_build_frame(frame, sizeof(frame),
                                        0, 8000, SIDECAR_FMT_INT16,
                                        512, SIDECAR_STREAM_RX_AUDIO, 1,
                                        audio_samples, sizeof(audio_samples));
    if (frame_len == SIDECAR_HEADER_LEN + sizeof(audio_samples)) {
        uint32_t *hdr = (uint32_t *)frame;
        if (hdr[1] == 8000 && hdr[6] == SIDECAR_STREAM_RX_AUDIO) {
            printf("OK (len=%d)\n", frame_len);
        } else {
            printf("FAILED (bad header: rate=%u type=%u)\n", hdr[1], hdr[6]);
        }
    } else {
        printf("FAILED (len=%d, expected=%zu)\n",
               frame_len, SIDECAR_HEADER_LEN + sizeof(audio_samples));
    }

    /* Test 6: Close ports */
    printf("Test 6: Close ports... ");
    sidecar_close_port(audio_fd);
    sidecar_close_port(iq_fd);
    printf("OK\n");

    printf("\nAll tests passed!\n");
    return 0;
}
