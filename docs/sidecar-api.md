# Hamlib Sidecar API Documentation

**Version:** 1.0  
**Date:** 2026-06-20

## Overview

The Hamlib Sidecar API provides a generic protocol for Hamlib backends to stream audio and IQ data to external sidecar processes. This enables:

- IQ demodulation for IQ-only radios (RSPduo, Airspy, LimeSDR, PlutoSDR)
- Audio processing (recording, streaming, analysis)
- Virtual soundcard creation for ham-radio applications
- Multi-application access to a single radio

## Architecture

```
┌─────────────┐
│ Radio / SDR │
└──────┬──────┘
       │ Native protocol (TCI, SoapySDR, etc.)
       ▼
┌─────────────────┐
│ Hamlib Backend  │
│   (uses lib     │
│    sidecar API) │
└──────┬──────────┘
       │ Sidecar binary frames (localhost TCP)
       ├──────────┬──────────┐
       ▼          ▼          ▼
  ┌─────────┐ ┌─────────┐ ┌──────────┐
  │ Audio   │ │ IQ      │ │ Smart    │
  │ Sidecar │ │ Sidecar │ │ Sidecar  │
  └────┬────┘ └────┬────┘ └────┬─────┘
       │           │            │
       ▼           ▼            ▼
  PulseAudio   GNU Radio   Demod + PA
       │           │            │
       ▼           ▼            ▼
  JS8Call,    Spectrum    WSJT-X, etc.
  fldigi,     display
  etc.
```

## Frame Protocol

### Frame Format

All communication uses a binary frame format:

```
┌─────────────────────────────┐
│ Header (64 bytes)           │ ◄─ 16 uint32 words, little-endian
├─────────────────────────────┤
│ Payload (0..N bytes)        │ ◄─ Audio/IQ samples OR empty
└─────────────────────────────┘
```

### Header Layout

| Offset | Word | Field        | Description |
|--------|------|--------------|-------------|
| 0      | [0]  | receiver     | TRX/VFO index (0-based) |
| 4      | [1]  | sample_rate  | Hz (0 for control frames) |
| 8      | [2]  | format       | Sample format (0-3) |
| 12     | [3]  | codec        | Reserved (must be 0) |
| 16     | [4]  | crc          | Reserved (must be 0) |
| 20     | [5]  | length       | Sample count OR control value |
| 24     | [6]  | stream_type  | Stream type (0-19) |
| 28     | [7]  | channels     | 1=mono, 2=stereo/IQ |
| 32-63  | [8-15] | reserved   | Must be zero |

### Sample Formats

| Value | Name | Bytes/Sample | Description |
|-------|------|--------------|-------------|
| 0 | INT16 | 2 | 16-bit signed integer |
| 1 | INT24 | 3 | 24-bit signed integer |
| 2 | INT32 | 4 | 32-bit signed integer |
| 3 | FLOAT32 | 4 | 32-bit IEEE float |

### Stream Types

#### Data Streams (with payload)

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| 0 | IQ | rigctld → sidecar | Raw I/Q samples (RX) |
| 1 | RX_AUDIO | rigctld → sidecar | Demodulated audio (RX) |
| 2 | TX_AUDIO | sidecar → rigctld | Audio to modulate (TX) |
| 3 | TX_CHRONO | rigctld → sidecar | TX pacing pulse (header-only) |
| 4 | PTT_STATE | rigctld → sidecar | PTT on/off (header-only) |
| 5 | TX_IQ | sidecar → rigctld | Modulated I/Q (TX) |

#### Control Frames (header-only, no payload)

| Value | Name | Fields | Description |
|-------|------|--------|-------------|
| 6 | MODE | length=rmode_t, channels=pbwidth | Mode change |
| 7 | FREQ | length=freq_lo, channels=freq_hi | Frequency (64-bit split) |
| 8 | SPLIT | length=enabled, channels=tx_vfo | Split VFO |
| 9 | FILTER | length=low_hz, channels=high_hz | Filter edges |
| 10 | AGC_LEVEL | length=agc_level (0-8) | AGC speed |
| 11 | NR_LEVEL | length=float_as_uint32 | Noise reduction (0.0-1.0) |
| 12 | NB_LEVEL | length=float_as_uint32 | Noise blanker (0.0-1.0) |
| 13 | NOTCH | length=freq_hz, channels=enable | Notch filter |
| 14 | RF_GAIN | length=float_as_uint32 | RF gain (0.0-1.0) |
| 15 | SQUELCH | length=float_as_uint32 | Squelch (0.0-1.0) |
| 16 | PREAMP | length=db | Preamp gain (0/10/20) |
| 17 | ATT | length=db | Attenuator (0/6/12/18) |
| 18 | CW_PITCH | length=hz | CW sidetone pitch |
| 19 | APF | length=float_as_uint32 | Audio peak filter (0.0-1.0) |

**Note:** Float values in control frames are reinterpreted as uint32 (bit-cast, not converted).

## Backend Integration Guide

### 1. Include Headers

```c
#include <hamlib/rig.h>
#include <hamlib/sidecar.h>
```

### 2. Add Sidecar State to Private Structure

```c
struct my_priv {
    /* Existing backend state... */
    
    /* Sidecar support */
    int audio_listen_fd;   /* Listener socket */
    int audio_client_fd;   /* Connected sidecar */
    int iq_listen_fd;      /* IQ listener */
    int iq_client_fd;      /* IQ sidecar */
};
```

### 3. Initialize Ports in Backend Open

```c
static int my_open(RIG *rig)
{
    struct my_priv *priv = (struct my_priv *)STATE(rig)->priv;
    
    /* Initialize audio sidecar port (optional) */
    if (priv->audio_port > 0) {
        priv->audio_listen_fd = sidecar_init_port(priv->audio_port);
        if (priv->audio_listen_fd < 0) {
            rig_debug(RIG_DEBUG_WARN, "sidecar audio port init failed\n");
            /* Continue without sidecar support */
        }
    }
    
    /* Initialize IQ sidecar port (optional) */
    if (priv->iq_port > 0) {
        priv->iq_listen_fd = sidecar_init_port(priv->iq_port);
        if (priv->iq_listen_fd < 0) {
            rig_debug(RIG_DEBUG_WARN, "sidecar IQ port init failed\n");
        }
    }
    
    /* Rest of backend initialization... */
}
```

### 4. Accept Sidecar Connections

Call `sidecar_accept_client()` periodically (e.g., in a background thread or before sending data):

```c
/* Check for new audio sidecar connection */
if (priv->audio_listen_fd >= 0) {
    int client_fd = sidecar_accept_client(priv->audio_listen_fd);
    if (client_fd >= 0) {
        /* New sidecar connected */
        if (priv->audio_client_fd >= 0) {
            close(priv->audio_client_fd);  /* Replace old connection */
        }
        priv->audio_client_fd = client_fd;
        rig_debug(RIG_DEBUG_VERBOSE, "audio sidecar connected\n");
    }
}
```

### 5. Forward Audio/IQ Data

```c
/* Example: Forward RX audio from radio to sidecar */
int16_t audio_samples[512];
size_t sample_count = radio_read_audio(audio_samples, 512);

sidecar_send_rx_audio(priv->audio_client_fd, 0, 8000,
                      SIDECAR_FMT_INT16, 1,
                      audio_samples, sample_count);

/* Example: Forward RX IQ from SDR */
float complex iq_samples[1024];
size_t iq_count = sdr_read_iq(iq_samples, 1024);

sidecar_send_rx_iq(priv->iq_client_fd, 0, 192000,
                   SIDECAR_FMT_FLOAT32,
                   iq_samples, iq_count);
```

### 6. Emit Control Frames

Forward CAT state changes to sidecars:

```c
static int my_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct my_priv *priv = (struct my_priv *)STATE(rig)->priv;
    
    /* Set mode on radio */
    int ret = radio_set_mode(mode, width);
    if (ret != RIG_OK) return ret;
    
    /* Notify sidecars */
    sidecar_emit_mode(priv->audio_client_fd, 0, mode, width);
    sidecar_emit_mode(priv->iq_client_fd, 0, mode, width);
    
    return RIG_OK;
}

static int my_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct my_priv *priv = (struct my_priv *)STATE(rig)->priv;
    
    /* Set frequency on radio */
    int ret = radio_set_freq(freq);
    if (ret != RIG_OK) return ret;
    
    /* Notify sidecars */
    int ch = (vfo == RIG_VFO_B) ? 1 : 0;
    sidecar_emit_freq(priv->audio_client_fd, ch, freq);
    sidecar_emit_freq(priv->iq_client_fd, ch, freq);
    
    return RIG_OK;
}

static int my_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct my_priv *priv = (struct my_priv *)STATE(rig)->priv;
    
    switch (level) {
    case RIG_LEVEL_AGC:
        radio_set_agc(val.i);
        sidecar_emit_agc_level(priv->audio_client_fd, 0, val.i);
        sidecar_emit_agc_level(priv->iq_client_fd, 0, val.i);
        break;
        
    case RIG_LEVEL_NR:
        radio_set_nr(val.f);
        sidecar_emit_nr_level(priv->audio_client_fd, 0, val.f);
        sidecar_emit_nr_level(priv->iq_client_fd, 0, val.f);
        break;
        
    /* ... other levels ... */
    }
    
    return RIG_OK;
}
```

### 7. Cleanup in Backend Close

```c
static int my_close(RIG *rig)
{
    struct my_priv *priv = (struct my_priv *)STATE(rig)->priv;
    
    /* Close sidecar connections */
    sidecar_close_port(priv->audio_client_fd);
    sidecar_close_port(priv->audio_listen_fd);
    sidecar_close_port(priv->iq_client_fd);
    sidecar_close_port(priv->iq_listen_fd);
    
    priv->audio_client_fd = -1;
    priv->audio_listen_fd = -1;
    priv->iq_client_fd = -1;
    priv->iq_listen_fd = -1;
    
    /* Rest of cleanup... */
    return RIG_OK;
}
```

## API Reference

See `include/hamlib/sidecar.h` for complete function documentation.

### Socket Management

- `sidecar_init_port(int port)` - Initialize TCP listener
- `sidecar_accept_client(int server_fd)` - Accept connection (non-blocking)
- `sidecar_close_port(int fd)` - Close socket

### Frame Building

- `sidecar_build_frame(...)` - Build raw frame (advanced use)

### Audio/IQ Streaming

- `sidecar_send_rx_audio(...)` - Send RX audio samples
- `sidecar_send_rx_iq(...)` - Send RX IQ samples

### Control Frame Emission

- `sidecar_emit_mode(...)` - Mode change
- `sidecar_emit_freq(...)` - Frequency change
- `sidecar_emit_split(...)` - Split VFO
- `sidecar_emit_filter(...)` - Filter edges
- `sidecar_emit_agc_level(...)` - AGC level
- `sidecar_emit_nr_level(...)` - Noise reduction
- `sidecar_emit_nb_level(...)` - Noise blanker
- `sidecar_emit_notch(...)` - Notch filter
- `sidecar_emit_rf_gain(...)` - RF gain
- `sidecar_emit_squelch(...)` - Squelch
- `sidecar_emit_preamp(...)` - Preamp
- `sidecar_emit_att(...)` - Attenuator
- `sidecar_emit_cw_pitch(...)` - CW pitch
- `sidecar_emit_apf(...)` - Audio peak filter
- `sidecar_emit_ptt_state(...)` - PTT state

## Best Practices

### Error Handling

Sidecar API functions return RIG_OK on success or negative error codes. Since sidecars are optional, most errors should be logged but not propagate:

```c
int ret = sidecar_send_rx_audio(...);
if (ret < 0) {
    rig_debug(RIG_DEBUG_WARN, "sidecar audio send failed: %d\n", ret);
    /* Don't return error - sidecar is optional */
}
```

### Thread Safety

The sidecar API is **not inherently thread-safe**. If your backend uses multiple threads:

- Ensure only one thread calls `sidecar_accept_client()` per listener
- Serialize access to client file descriptors with mutexes if multiple threads send data

### Performance

- Sidecar sockets are **localhost TCP** and very low overhead
- Emission functions are non-blocking (MSG_NOSIGNAL)
- Failed sends (EAGAIN/EWOULDBLOCK) drop frames silently
- No buffering overhead - frames sent directly

### Disconnection Handling

When a sidecar disconnects (EPIPE/ECONNRESET):

1. The send function returns -RIG_EIO
2. Backend should close the client fd: `close(priv->audio_client_fd); priv->audio_client_fd = -1;`
3. Continue accepting new connections via `sidecar_accept_client()`

### Configuration

Typical backend configuration parameters:

```c
#define TOK_MY_AUDIO_PORT TOKEN_BACKEND(100)
#define TOK_MY_IQ_PORT    TOKEN_BACKEND(101)

static const struct confparams my_cfg_params[] = {
    {
        TOK_MY_AUDIO_PORT, "audio_port", "Audio sidecar port", "Port",
        "4534", RIG_CONF_NUMERIC, { .n = { 0, 65535, 1 } }
    },
    {
        TOK_MY_IQ_PORT, "iq_port", "IQ sidecar port", "Port",
        "4535", RIG_CONF_NUMERIC, { .n = { 0, 65535, 1 } }
    },
    { RIG_CONF_END, NULL, }
};
```

Usage: `rigctld -m <model> -C audio_port=4534 -C iq_port=4535 ...`

## Examples

### Minimal Backend with Audio Support

```c
#include <hamlib/rig.h>
#include <hamlib/sidecar.h>

struct minimal_priv {
    int audio_fd;
};

static int minimal_open(RIG *rig)
{
    struct minimal_priv *priv = (struct minimal_priv *)STATE(rig)->priv;
    
    /* Initialize sidecar port */
    priv->audio_fd = sidecar_init_port(4534);
    
    return RIG_OK;
}

static int minimal_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct minimal_priv *priv = (struct minimal_priv *)STATE(rig)->priv;
    
    /* Set mode on radio (your radio-specific code) */
    /* ... */
    
    /* Notify sidecar */
    sidecar_emit_mode(priv->audio_fd, 0, mode, width);
    
    return RIG_OK;
}

static int minimal_close(RIG *rig)
{
    struct minimal_priv *priv = (struct minimal_priv *)STATE(rig)->priv;
    
    sidecar_close_port(priv->audio_fd);
    
    return RIG_OK;
}
```

### Full Integration Example

See `rigs/dummy/tci2.c` for a complete production implementation including:

- Bidirectional audio (RX/TX)
- IQ streaming
- All 14 control frame types
- Background thread with poll()
- Connection management
- Error handling

## Sidecar Implementations

Reference sidecar implementations live in the `hamlib-audio-sidecar`
repository (https://github.com/jfrancis42/hamlib-audio-sidecar):

- **`hamlib_sidecar_linux.py`** — Linux PulseAudio sidecar. Two
  user-side backends: virtual PulseAudio audio sinks (built-in
  demodulator + DSP for IQ-only backends), or IQ-as-stereo soundcard.
- **`hamlib_sidecar_portable.py`** — any-OS sidecar. GNU Radio ZMQ
  endpoints (audio + IQ, RX + TX) or direct SoapySDR hardware bridge.
- **`hamctl`** — runtime control CLI (frequency, mode, DSP levels,
  S-meter, audio routing, favorites, band plan).
- **`hamlib.sh`** — lifecycle script for the Linux sidecar.

All four sit on top of one shared library
(`hamlib_sidecar_common.py`) which is the canonical Python
implementation of the wire protocol.

## Protocol Evolution

### Adding New Stream Types

When adding new stream types (20+):

1. Define constant in `sidecar.h`: `#define SIDECAR_STREAM_NEWTYPE 20`
2. Add emission function: `sidecar_emit_newtype(...)`
3. Implement in `sidecar.c`
4. Update this documentation
5. Update sidecar implementations to **ignore unknown types** (required for compatibility)

### Backwards Compatibility

**Critical rule:** Receivers MUST silently skip unknown stream types.

This ensures:
- Old sidecars work with new backends
- New sidecars work with old backends (for types 0-19)
- Protocol can evolve without version negotiation

## Testing

### Test Sidecar Connection

```bash
# Start rigctld with sidecar ports
rigctld -m <model> -C audio_port=4534 -C iq_port=4535

# Connect with netcat to verify port is listening
nc localhost 4534
# (Will accept connection, then receive binary frames)
```

### Debug Frame Protocol

```python
import socket, struct

s = socket.create_connection(('localhost', 4534))

while True:
    hdr = s.recv(64)
    if len(hdr) < 64: break
    
    fields = struct.unpack('<16I', hdr)
    print(f"RX: recv={fields[0]} rate={fields[1]} fmt={fields[2]} "
          f"len={fields[5]} type={fields[6]} ch={fields[7]}")
    
    # Read payload if present
    # (calculate size based on format/length/channels)
```

## References

- **Header:** `include/hamlib/sidecar.h`
- **Implementation:** `src/sidecar.c`
- **Example Backend:** `rigs/dummy/tci2.c`
- **Sidecar Repository:** https://github.com/jfrancis42/hamlib-audio-sidecar
- **Protocol Spec (canonical):** `PROTOCOL.md` in the sidecar repo
- **User Guide:** `USERS.md` in the sidecar repo
- **Developer Guide:** `DEVELOPERS.md` in the sidecar repo

## Backend-Specific Considerations

### TX Pacing Models

Different radios use different TX flow control. The sidecar API supports multiple models:

#### Pull-Based (TX_CHRONO)

**Model:** Radio tells client "send N samples now"  
**Stream Type:** SIDECAR_STREAM_TX_CHRONO (type 3)  
**Used By:** TCI, some network radios

```c
// Backend sends CHRONO frames periodically
uint32_t samples_wanted = 512;
sidecar_emit_tx_chrono(priv->audio_fd, 0, samples_wanted);
// Sidecar responds with TX_AUDIO/TX_IQ containing exactly 512 samples
```

#### Buffer-Level Based

**Model:** Radio reports buffer level, client maintains target level  
**Stream Type:** SIDECAR_STREAM_TX_BUFFER_LEVEL (type 20)  
**Used By:** FlexRadio SmartSDR  
**Status:** Planned (type 20 reserved)

#### Free-Running

**Model:** Client sends continuously, no pacing frames  
**Used By:** Simple radios, KiwiSDR  
**Implementation:** Just don't send CHRONO - sidecar free-runs

#### Timestamp-Based

**Model:** Samples tagged with transmit timestamps  
**Stream Type:** SIDECAR_STREAM_TX_SCHEDULED (type 21)  
**Used By:** VITA-49 (FlexRadio), GPS-disciplined  
**Status:** Planned (type 21 reserved)

### Metadata Support

Some SDRs provide additional metadata beyond audio/IQ:

#### GPS Timestamps

**Use Case:** KiwiSDR, Airspy HF+, precision receivers  
**Stream Type:** SIDECAR_STREAM_METADATA (type 22)  
**Status:** Planned (type 22 reserved)

```c
// Future API:
sidecar_emit_timestamp(fd, receiver, gps_timestamp_ns);
```

#### Signal Level (RSSI)

**Use Case:** Most SDRs provide signal strength  
**Stream Type:** SIDECAR_STREAM_METADATA (type 22)  
**Status:** Planned

```c
// Future API:
sidecar_emit_signal_level(fd, receiver, rssi_dbfs);
```

### Spectrum Data

**Use Case:** Panadapter, waterfall display  
**Stream Type:** SIDECAR_STREAM_SPECTRUM (type 24)  
**Status:** Planned (type 24 reserved)

Backends with built-in FFT (KiwiSDR, FlexRadio, etc.) can stream spectrum data:

```c
// Future API:
sidecar_send_spectrum(fd, receiver, center_freq, span_hz,
                      fft_bins, bin_count, update_rate);
```

### Multi-Receiver SDRs

**Use Case:** RSPduo, FlexRadio (multiple "slices"), Perseus  
**Implementation:** Use `receiver` field as index

```c
// Tuner A
sidecar_send_rx_iq(fd, 0, 8000000, SIDECAR_FMT_INT16, iq_a, 8192);

// Tuner B
sidecar_send_rx_iq(fd, 1, 8000000, SIDECAR_FMT_INT16, iq_b, 8192);
```

**No protocol changes needed** - already supported!

### Sample Rate Constraints

Different SDRs have different rate constraints:

| Radio | Rates | Notes |
|-------|-------|-------|
| KiwiSDR | 12000 Hz fixed | No negotiation |
| SDRPlay | 2-10 MHz | Continuous range |
| FlexRadio | 24/48/96/192 kHz | Discrete set |
| RTL-SDR | 225k-3.2 MHz | Continuous range |

**Current:** Backend dictates rate, sidecar adapts (resampling)  
**Future:** SIDECAR_STREAM_CAPABILITY (type 23) for rate negotiation

### Vendor-Specific Extensions

**Stream Types 1000-1999** reserved for vendor extensions:

- 1000: FlexRadio VITA-49 extensions
- 1001: ICOM proprietary features
- 1002: KiwiSDR extensions
- 1003: SDRPlay extensions
- etc.

Backends may define custom stream types in this range. Sidecars MUST ignore unknown vendor types.

### Backend Examples

#### TCI (ExpertSDR3)
- **Audio:** 48 kHz int16 mono
- **IQ:** 48/96/192 kHz float32
- **TX Pacing:** TX_CHRONO (type 3)
- **Control:** All 14 types supported

#### KiwiSDR (Future)
- **Audio:** 12 kHz int16 mono (fixed rate)
- **IQ:** Optional (12 kHz)
- **TX Pacing:** N/A (RX-only)
- **Metadata:** GPS timestamps (type 22)

#### FlexRadio SmartSDR (Future)
- **Audio:** 24/48 kHz int16
- **IQ:** 24/48/96/192 kHz float32
- **TX Pacing:** Buffer level (type 20)
- **Metadata:** Timestamps (type 22), spectrum (type 24)

#### SDRPlay RSPduo (Future)
- **Audio:** N/A (IQ-only)
- **IQ:** 2-10 MHz int16/float32
- **Multi-receiver:** Yes (receiver=0/1)
- **Capability:** Rate negotiation (type 23)

#### ICOM IC-7300 (Future)
- **Audio:** 48 kHz int16 mono
- **IQ:** Not available (native radio)
- **TX Pacing:** Free-running
- **Control:** Standard 14 types

## Support

- **Issues:** https://github.com/Hamlib/Hamlib/issues
- **Mailing List:** hamlib-developer@lists.sourceforge.net
- **Wiki:** https://github.com/Hamlib/Hamlib/wiki
