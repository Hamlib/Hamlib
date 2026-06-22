/*
 *  Hamlib Sidecar API
 *  Copyright (c) 2026 by Jeff Francis N0GQ
 *
 *  Generic API for Hamlib backends to communicate with audio/IQ sidecars.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

/**
 * \file sidecar.h
 * \brief Hamlib sidecar API for audio/IQ streaming
 *
 * This API provides a generic protocol for Hamlib backends to stream audio
 * and IQ data to external sidecar processes for demodulation, recording, or
 * processing.
 *
 * ## Architecture
 *
 * ```
 * Radio/SDR ↔ Hamlib Backend ↔ rigctld ↔ Sidecar(s) ↔ Applications
 *                   |                         |
 *                   | libsidecar API          | Virtual audio devices
 *                   |                         | or ZMQ/network streams
 * ```
 *
 * ## Frame Protocol
 *
 * All communication uses a binary frame format:
 * - 64-byte header (16 uint32 words, little-endian)
 * - 0..N bytes payload (audio/IQ samples)
 *
 * Header layout:
 * ```
 * [0]  receiver    - TRX/VFO index
 * [1]  sample_rate - Hz
 * [2]  format      - SIDECAR_FMT_*
 * [3]  codec       - Reserved (0)
 * [4]  crc         - Reserved (0)
 * [5]  length      - Sample count OR control value
 * [6]  stream_type - SIDECAR_STREAM_*
 * [7]  channels    - 1=mono, 2=stereo/IQ
 * [8-15] reserved  - Must be zero
 * ```
 *
 * ## Stream Types
 *
 * - **0-5:** Audio/IQ data streams (with payload)
 * - **6-19:** Control frames (header-only, no payload)
 *
 * ## Backend Usage
 *
 * ### Initialization
 * ```c
 * #include <hamlib/sidecar.h>
 *
 * struct my_priv {
 *     int audio_fd;  // File descriptor for audio sidecar
 *     int iq_fd;     // File descriptor for IQ sidecar
 * };
 *
 * static int my_open(RIG *rig)
 * {
 *     struct my_priv *priv = ...;
 *
 *     // Initialize sidecar ports (listeners on localhost)
 *     priv->audio_fd = sidecar_init_port(4534);  // Audio port
 *     priv->iq_fd = sidecar_init_port(4535);     // IQ port
 *
 *     if (priv->audio_fd < 0 || priv->iq_fd < 0) {
 *         // Sidecar ports optional, continue without them
 *     }
 * }
 * ```
 *
 * ### Streaming Audio
 * ```c
 * // Forward RX audio from radio to sidecar
 * int16_t audio_samples[512];
 * radio_read_audio(audio_samples, 512);
 *
 * sidecar_send_rx_audio(priv->audio_fd, 0, 8000,
 *                       SIDECAR_FMT_INT16, 1,
 *                       audio_samples, 512);
 * ```
 *
 * ### Streaming IQ
 * ```c
 * // Forward RX IQ from SDR to sidecar
 * float complex iq_samples[1024];
 * sdr_read_iq(iq_samples, 1024);
 *
 * sidecar_send_rx_iq(priv->iq_fd, 0, 192000,
 *                    SIDECAR_FMT_FLOAT32, 2,
 *                    iq_samples, 1024);
 * ```
 *
 * ### Control Frames
 * ```c
 * // Notify sidecar of mode change
 * static int my_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
 * {
 *     // Set mode on radio
 *     radio_set_mode(mode);
 *
 *     // Forward to sidecar
 *     sidecar_emit_mode(priv->audio_fd, 0, mode, width);
 *     sidecar_emit_mode(priv->iq_fd, 0, mode, width);
 *
 *     return RIG_OK;
 * }
 * ```
 *
 * ### Cleanup
 * ```c
 * static int my_close(RIG *rig)
 * {
 *     struct my_priv *priv = ...;
 *
 *     sidecar_close_port(priv->audio_fd);
 *     sidecar_close_port(priv->iq_fd);
 *
 *     return RIG_OK;
 * }
 * ```
 *
 * ## See Also
 *
 * - rigs/dummy/tci2.c - Reference implementation
 * - docs/sidecar-protocol.md - Complete protocol specification
 * - hamlib-audio-sidecar repository - Sidecar implementations
 */

#ifndef _SIDECAR_H
#define _SIDECAR_H 1

#include <hamlib/rig.h>
#include <stdint.h>
#include <stddef.h>

__BEGIN_DECLS

/* -------------------------------------------------------------------------
 * Frame Format Constants
 * ----------------------------------------------------------------------- */

/** Frame header length in bytes (16 uint32 words) */
#define SIDECAR_HEADER_LEN 64

/** Stream type: IQ samples (RX) */
#define SIDECAR_STREAM_IQ        0
/** Stream type: RX audio (demodulated) */
#define SIDECAR_STREAM_RX_AUDIO  1
/** Stream type: TX audio (to modulate) */
#define SIDECAR_STREAM_TX_AUDIO  2
/** Stream type: TX pacing (header-only) */
#define SIDECAR_STREAM_TX_CHRONO 3
/** Stream type: PTT state (header-only) */
#define SIDECAR_STREAM_PTT_STATE 4
/** Stream type: TX IQ (modulated, from sidecar) */
#define SIDECAR_STREAM_TX_IQ     5

/* Control frames (header-only, no payload) */
/** Control: Mode change */
#define SIDECAR_STREAM_MODE      6
/** Control: Frequency change */
#define SIDECAR_STREAM_FREQ      7
/** Control: Split enable/disable */
#define SIDECAR_STREAM_SPLIT     8
/** Control: Filter edges */
#define SIDECAR_STREAM_FILTER    9
/** Control: AGC level */
#define SIDECAR_STREAM_AGC_LEVEL 10
/** Control: Noise reduction level */
#define SIDECAR_STREAM_NR_LEVEL  11
/** Control: Noise blanker level */
#define SIDECAR_STREAM_NB_LEVEL  12
/** Control: Notch filter frequency */
#define SIDECAR_STREAM_NOTCH     13
/** Control: RF gain */
#define SIDECAR_STREAM_RF_GAIN   14
/** Control: Squelch level */
#define SIDECAR_STREAM_SQUELCH   15
/** Control: Preamp gain */
#define SIDECAR_STREAM_PREAMP    16
/** Control: Attenuator */
#define SIDECAR_STREAM_ATT       17
/** Control: CW pitch */
#define SIDECAR_STREAM_CW_PITCH  18
/** Control: Audio peak filter */
#define SIDECAR_STREAM_APF       19

/* Extended stream types for non-TCI backends */
/** TX buffer level pacing (alternative to TX_CHRONO) */
#define SIDECAR_STREAM_TX_BUFFER_LEVEL 20
/** TX scheduled with timestamp (VITA-49 style) */
#define SIDECAR_STREAM_TX_SCHEDULED    21
/** Metadata (timestamps, signal levels, etc.) */
#define SIDECAR_STREAM_METADATA        22
/** Backend capabilities announcement */
#define SIDECAR_STREAM_CAPABILITY      23
/** Spectrum / FFT data */
#define SIDECAR_STREAM_SPECTRUM        24
/** Control: FM peak deviation in Hz (e.g. 5000 for NBFM, 75000 for broadcast WFM) */
#define SIDECAR_STREAM_FM_DEVIATION    25

/* Reserved ranges */
/** First reserved stream type (for future Hamlib use) */
#define SIDECAR_STREAM_RESERVED_START  100
/** Last reserved stream type */
#define SIDECAR_STREAM_RESERVED_END    999
/** First vendor-specific stream type */
#define SIDECAR_STREAM_VENDOR_START    1000
/** Last vendor-specific stream type */
#define SIDECAR_STREAM_VENDOR_END      1999

/** Sample format: 16-bit signed integer */
#define SIDECAR_FMT_INT16   0
/** Sample format: 24-bit signed integer */
#define SIDECAR_FMT_INT24   1
/** Sample format: 32-bit signed integer */
#define SIDECAR_FMT_INT32   2
/** Sample format: 32-bit float */
#define SIDECAR_FMT_FLOAT32 3

/* -------------------------------------------------------------------------
 * Protocol Compatibility Rules
 * ----------------------------------------------------------------------- */

/**
 * CRITICAL COMPATIBILITY REQUIREMENT:
 *
 * All sidecar implementations MUST silently skip unknown stream types.
 * This ensures:
 * - Old sidecars work with new backends
 * - New sidecars work with old backends (for types 0-19)
 * - Protocol can evolve without version negotiation
 *
 * When implementing a sidecar:
 * ```c
 * switch (stream_type) {
 * case SIDECAR_STREAM_IQ:
 *     handle_iq(...);
 *     break;
 * // ... handle known types ...
 * default:
 *     // REQUIRED: Silently skip unknown types
 *     break;
 * }
 * ```
 *
 * VENDOR EXTENSIONS:
 * - Types 1000-1999 are reserved for vendor-specific extensions
 * - Format: 1000 + vendor_id (allocate from Hamlib wiki)
 * - Example: FlexRadio VITA-49 extensions = 1000
 * - Sidecars MUST skip vendor extensions they don't recognize
 */


/* -------------------------------------------------------------------------
 * Socket Management
 * ----------------------------------------------------------------------- */

/**
 * \brief Initialize a sidecar listener port
 *
 * Creates a TCP listener on localhost:port for sidecar connections.
 * Non-blocking, accepts one client at a time.
 *
 * \param port TCP port number (e.g., 4534 for audio, 4535 for IQ)
 * \return File descriptor on success, -1 on error
 *
 * Example:
 * ```c
 * int audio_fd = sidecar_init_port(4534);
 * if (audio_fd < 0) {
 *     // No sidecar support, continue without it
 * }
 * ```
 */
extern HAMLIB_EXPORT(int) sidecar_init_port(int port);

/**
 * \brief Accept a sidecar client connection
 *
 * Non-blocking accept on a listener socket created by sidecar_init_port().
 * Call periodically to accept new connections.
 *
 * \param server_fd Listener socket from sidecar_init_port()
 * \return Client file descriptor on success, -1 if no client waiting
 *
 * Example:
 * ```c
 * int client_fd = sidecar_accept_client(audio_fd);
 * if (client_fd >= 0) {
 *     // New sidecar connected
 *     close(old_client_fd);
 *     audio_client_fd = client_fd;
 * }
 * ```
 */
extern HAMLIB_EXPORT(int) sidecar_accept_client(int server_fd);

/**
 * \brief Close a sidecar port
 *
 * Closes listener or client socket.
 *
 * \param fd File descriptor to close
 */
extern HAMLIB_EXPORT(void) sidecar_close_port(int fd);


/* -------------------------------------------------------------------------
 * Frame Building
 * ----------------------------------------------------------------------- */

/**
 * \brief Build a sidecar frame
 *
 * Constructs a 64-byte header + optional payload.
 *
 * \param buf Output buffer (must be >= SIDECAR_HEADER_LEN + payload_len)
 * \param buflen Buffer size
 * \param receiver TRX/VFO index (0-based)
 * \param sample_rate Sample rate in Hz (0 for control frames)
 * \param format Sample format (SIDECAR_FMT_*)
 * \param length Sample count (data frames) or control value (control frames)
 * \param stream_type Stream type (SIDECAR_STREAM_*)
 * \param channels Channel count (1=mono, 2=stereo/IQ)
 * \param payload Payload bytes (NULL for control frames)
 * \param payload_len Payload length in bytes
 * \return Total frame length (header + payload) on success, -1 on error
 *
 * Example:
 * ```c
 * uint8_t frame[1088];  // 64-byte header + 1024 bytes audio
 * int16_t audio[512];
 * int frame_len = sidecar_build_frame(frame, sizeof(frame),
 *                                     0, 8000, SIDECAR_FMT_INT16,
 *                                     512, SIDECAR_STREAM_RX_AUDIO, 1,
 *                                     audio, sizeof(audio));
 * send(fd, frame, frame_len, 0);
 * ```
 */
extern HAMLIB_EXPORT(int) sidecar_build_frame(
    uint8_t *buf, size_t buflen,
    uint32_t receiver, uint32_t sample_rate,
    uint32_t format, uint32_t length,
    uint32_t stream_type, uint32_t channels,
    const void *payload, size_t payload_len);


/* -------------------------------------------------------------------------
 * Audio/IQ Streaming
 * ----------------------------------------------------------------------- */

/**
 * \brief Send RX audio samples to sidecar
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param sample_rate Sample rate in Hz
 * \param format Sample format (SIDECAR_FMT_*)
 * \param channels Channel count (1=mono, 2=stereo)
 * \param samples Audio samples
 * \param sample_count Number of samples
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_send_rx_audio(
    int fd, uint32_t receiver, uint32_t sample_rate,
    uint32_t format, uint32_t channels,
    const void *samples, size_t sample_count);

/**
 * \brief Send RX IQ samples to sidecar
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param sample_rate Sample rate in Hz
 * \param format Sample format (SIDECAR_FMT_*)
 * \param samples IQ samples (interleaved I,Q,I,Q or complex)
 * \param sample_count Number of I/Q pairs
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_send_rx_iq(
    int fd, uint32_t receiver, uint32_t sample_rate,
    uint32_t format,
    const void *samples, size_t sample_count);


/* -------------------------------------------------------------------------
 * Control Frame Emission
 * ----------------------------------------------------------------------- */

/**
 * \brief Emit mode change control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param mode Hamlib mode (rmode_t)
 * \param width Passband width in Hz
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_mode(
    int fd, uint32_t receiver, rmode_t mode, pbwidth_t width);

/**
 * \brief Emit frequency change control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param freq Frequency in Hz
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_freq(
    int fd, uint32_t receiver, freq_t freq);

/**
 * \brief Emit split enable/disable control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param split Split state (RIG_SPLIT_ON/OFF)
 * \param tx_vfo TX VFO (RIG_VFO_A/B)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_split(
    int fd, uint32_t receiver, split_t split, vfo_t tx_vfo);

/**
 * \brief Emit filter edges control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param low_hz Filter low edge in Hz (negative for LSB)
 * \param high_hz Filter high edge in Hz (positive for USB)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_filter(
    int fd, uint32_t receiver, int low_hz, int high_hz);

/**
 * \brief Emit AGC level control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param agc_level AGC level (agc_level_e: 0=OFF, 1=SUPERFAST, ..., 8=ON)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_agc_level(
    int fd, uint32_t receiver, int agc_level);

/**
 * \brief Emit noise reduction level control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param nr_level Noise reduction level (0.0 = off, 1.0 = maximum)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_nr_level(
    int fd, uint32_t receiver, float nr_level);

/**
 * \brief Emit noise blanker level control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param nb_level Noise blanker level (0.0 = off, 1.0 = maximum)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_nb_level(
    int fd, uint32_t receiver, float nb_level);

/**
 * \brief Emit notch filter control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param notch_hz Notch frequency in Hz (0 = off)
 * \param enable Enable flag (0 = off, 1 = on)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_notch(
    int fd, uint32_t receiver, int notch_hz, int enable);

/**
 * \brief Emit RF gain control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param rf_gain RF gain (0.0 = minimum, 1.0 = maximum)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_rf_gain(
    int fd, uint32_t receiver, float rf_gain);

/**
 * \brief Emit squelch level control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param squelch Squelch level (0.0 = off, 1.0 = maximum)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_squelch(
    int fd, uint32_t receiver, float squelch);

/**
 * \brief Emit preamp gain control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param preamp_db Preamp gain in dB (0, 10, 20, etc.)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_preamp(
    int fd, uint32_t receiver, int preamp_db);

/**
 * \brief Emit attenuator control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param att_db Attenuator in dB (0, 6, 12, 18, etc.)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_att(
    int fd, uint32_t receiver, int att_db);

/**
 * \brief Emit CW pitch control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param pitch_hz CW sidetone pitch in Hz (400-1000 typical)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_cw_pitch(
    int fd, uint32_t receiver, int pitch_hz);

/**
 * \brief Emit audio peak filter control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param apf_level APF level (0.0 = off, 1.0 = maximum)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_apf(
    int fd, uint32_t receiver, float apf_level);

/**
 * \brief Emit PTT state control frame
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param ptt_on PTT state (0 = off, 1 = on)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_ptt_state(
    int fd, uint32_t receiver, int ptt_on);

/**
 * \brief Emit FM peak deviation control frame
 *
 * Tells the sidecar what FM peak deviation to assume when demodulating.
 * Lets the backend keep all standards-knowledge ("WFM broadcast = 75 kHz,
 * NBFM = 5 kHz") on the Hamlib side rather than forcing every sidecar to
 * carry an FM-deviation table.
 *
 * \param fd Sidecar connection file descriptor
 * \param receiver TRX/VFO index
 * \param deviation_hz Peak deviation in Hz (e.g. 5000, 75000)
 * \return RIG_OK on success, negative error code on failure
 */
extern HAMLIB_EXPORT(int) sidecar_emit_fm_deviation(
    int fd, uint32_t receiver, int deviation_hz);

__END_DECLS

#endif /* _SIDECAR_H */
