# Hamlib Audio/IQ Sidecar Architecture

**Version:** 1.0 (2026-06-20)

---

## Overview

The Hamlib sidecar architecture enables external processes (sidecars) to handle audio/IQ streaming, DSP, and platform-specific integrations for radio backends. This keeps Hamlib's core pure C with minimal dependencies while enabling rich audio features via external Python/C++ sidecars.

---

## Why Sidecars?

### Problem: Audio is Platform-Dependent

Traditional approaches require embedding audio libraries in Hamlib:
- **Linux**: PulseAudio / PipeWire / ALSA
- **Windows**: WASAPI / DirectSound / MME
- **macOS**: CoreAudio

Each has different APIs, threading models, and failure modes.

### Solution: External Sidecars

```
Radio/SDR ↔ Hamlib Backend ↔ rigctld ↔ Sidecar(s) ↔ Applications
                  |                         |
                  | Generic binary protocol | Platform audio APIs
                  |                         | or ZMQ/network streams
```

**Benefits:**
- **No new Hamlib dependencies** — rigctld stays pure C
- **Platform flexibility** — Linux/Windows/macOS sidecars use different audio APIs without Hamlib rebuilds
- **Process isolation** — audio/IQ bugs don't crash rigctld
- **Fast iteration** — audio pipeline changes are Python edits + restart, not Hamlib rebuild
- **Language freedom** — sidecars can be Python, C++, Rust, Go, etc.
- **Multiple consumers** — one rigctld can feed multiple sidecars (audio, IQ, recording, analysis)

---

## Protocol

### Binary Frame Format

All communication uses a fixed-header binary frame format:

```
┌─────────────────────────────┐
│ Header (64 bytes)           │ ← 16 uint32 words, little-endian
├─────────────────────────────┤
│ Payload (0..N bytes)        │ ← Audio/IQ samples OR empty (control frames)
└─────────────────────────────┘
```

### Header Layout

| Offset | Word | Field        | Description |
|--------|------|--------------|-------------|
| 0      | [0]  | receiver     | TRX/VFO index (0-based) |
| 4      | [1]  | sample_rate  | Hz (0 for control frames) |
| 8      | [2]  | format       | 0=int16, 1=int24, 2=int32, 3=float32 |
| 12     | [3]  | codec        | Reserved (0) |
| 16     | [4]  | crc          | Reserved (0) |
| 20     | [5]  | length       | Sample count OR control value |
| 24     | [6]  | stream_type  | Stream type (0-24, plus reserved ranges) |
| 28     | [7]  | channels     | 1=mono, 2=stereo/IQ |
| 32-63  | [8-15] | reserved   | Must be zero (future extensions) |

### Stream Types

#### Data Streams (with payload)

| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0 | IQ | rigctld → sidecar | Raw I/Q samples (RX) |
| 1 | RX_AUDIO | rigctld → sidecar | Demodulated audio (RX) |
| 2 | TX_AUDIO | sidecar → rigctld | Audio to modulate (TX) |
| 3 | TX_CHRONO | rigctld → sidecar | TX pacing pulse (header-only) |
| 4 | PTT_STATE | rigctld → sidecar | PTT on/off (header-only) |
| 5 | TX_IQ | sidecar → rigctld | Modulated I/Q (TX) |

#### Control Frames (header-only, no payload)

| Type | Name | Description |
|------|------|-------------|
| 6 | MODE | Mode change (SSB, CW, DIGI, etc.) |
| 7 | FREQ | Frequency change |
| 8 | SPLIT | Split VFO enable/disable |
| 9 | FILTER | Filter edges (low/high Hz) |
| 10 | AGC_LEVEL | AGC speed |
| 11 | NR_LEVEL | Noise reduction (0.0-1.0) |
| 12 | NB_LEVEL | Noise blanker (0.0-1.0) |
| 13 | NOTCH | Notch filter frequency |
| 14 | RF_GAIN | RF gain (0.0-1.0) |
| 15 | SQUELCH | Squelch level (0.0-1.0) |
| 16 | PREAMP | Preamp gain (dB) |
| 17 | ATT | Attenuator (dB) |
| 18 | CW_PITCH | CW sidetone pitch (Hz) |
| 19 | APF | Audio peak filter (0.0-1.0) |

#### Extended Types (for non-TCI backends)

| Type | Name | Description | Status |
|------|------|-------------|--------|
| 20 | TX_BUFFER_LEVEL | Buffer-level TX pacing | Reserved |
| 21 | TX_SCHEDULED | Timestamp-based TX | Reserved |
| 22 | METADATA | GPS timestamps, RSSI | Reserved |
| 23 | CAPABILITY | Backend capabilities | Reserved |
| 24 | SPECTRUM | FFT / panadapter data | Reserved |

#### Reserved Ranges

| Range | Purpose |
|-------|---------|
| 100-999 | Future Hamlib use |
| 1000-1999 | Vendor-specific extensions |

**Critical compatibility rule:** Sidecars MUST silently skip unknown stream types.

---

## Architecture Patterns

### Pattern 1: Direct Audio (TCI, ICOM)

Backend provides demodulated audio → sidecar bridges to OS audio system:

```
Radio → rigctld → Audio Sidecar → PulseAudio/CoreAudio/WASAPI
                  (STREAM_RX_AUDIO)    ↓
                                   JS8Call, fldigi, etc.
```

**Backends:** TCI, ICOM IC-7300/IC-705, any radio with built-in demodulator

**Sidecar example:**
- `hamlib_sidecar_linux.py --user-backend pulseaudio` (Linux, default).
  Windows / macOS native PulseAudio replacements are planned.

### Pattern 2: IQ → Demod in Sidecar (SoapySDR, Perseus, Airspy)

Backend provides raw IQ → sidecar demodulates and bridges to audio:

```
SDR → rigctld → Smart Sidecar → PulseAudio/CoreAudio/WASAPI
      (STREAM_IQ)  (IQ demod)       ↓
                                modem apps
```

**Backends:** SoapySDR (RTL-SDR, HackRF, LimeSDR, PlutoSDR), Perseus, Airspy, SDRPlay

**Sidecar example:**
- `hamlib_sidecar_linux.py --user-backend pulseaudio` connected to the
  IQ port. The same demod/DSP suite (`hamlib_sidecar_common.py`) drives
  both the audio-input case and the IQ-input case — when it sees IQ
  frames it engages SSB / AM / FM mono+stereo / CW demodulators and the
  full DSP suite (AGC, NR, NB, notch, APF).

### Pattern 3: IQ for Direct Processing (GNU Radio, SDR apps)

Backend provides raw IQ → sidecar streams to SDR software:

```
SDR → rigctld → IQ Sidecar → ZMQ PUB → GNU Radio / Inspectrum / etc.
      (STREAM_IQ)              (complex float32)
```

**Backends:** Any IQ-capable backend

**Sidecar example:**
- `hamlib_sidecar_portable.py --user-backend gnuradio` — ZMQ bridge for
  audio RX/TX + IQ RX/TX (four endpoints).
- `hamlib_sidecar_portable.py --user-backend soapysdr` — direct SoapySDR
  to Hamlib bridge, bypassing rigctld.

### Pattern 4: Dual Audio + IQ (Advanced)

Backend provides both audio and IQ → independent sidecars:

```
Radio → rigctld → Audio Sidecar (port 4534) → Modem apps
                → IQ Sidecar (port 4535) → GNU Radio / Panadapter
```

**Use case:** Run modem on demodulated audio while simultaneously viewing spectrum/waterfall

---

## Backend Integration

### Configuration Parameters

Typical backend config for sidecar support:

```c
#define TOK_AUDIO_PORT TOKEN_BACKEND(100)
#define TOK_IQ_PORT    TOKEN_BACKEND(101)

static const struct confparams backend_cfg_params[] = {
    {
        TOK_AUDIO_PORT, "audio_port", "Audio sidecar port", "Port",
        "4534", RIG_CONF_NUMERIC, { .n = { 0, 65535, 1 } }
    },
    {
        TOK_IQ_PORT, "iq_port", "IQ sidecar port", "Port",
        "4535", RIG_CONF_NUMERIC, { .n = { 0, 65535, 1 } }
    },
    { RIG_CONF_END, NULL, }
};
```

Usage:
```bash
rigctld -m <model> -C audio_port=4534 -C iq_port=4535 ...
```

### API Usage

Backends use the sidecar API from `<hamlib/sidecar.h>`:

```c
#include <hamlib/sidecar.h>

// Initialize port (creates TCP listener on localhost)
priv->audio_fd = sidecar_init_port(4534);

// Forward audio to sidecar
sidecar_send_rx_audio(priv->audio_fd, 0, 8000, 
                      SIDECAR_FMT_INT16, 1, audio, 512);

// Forward IQ to sidecar  
sidecar_send_rx_iq(priv->iq_fd, 0, 192000,
                   SIDECAR_FMT_FLOAT32, iq, 1024);

// Notify sidecars of CAT changes
sidecar_emit_mode(priv->audio_fd, 0, mode, width);
sidecar_emit_freq(priv->audio_fd, 0, freq);
sidecar_emit_agc_level(priv->audio_fd, 0, agc);
// ... etc (15 control frame types)

// Cleanup
sidecar_close_port(priv->audio_fd);
```

**Complete integration guide:** `docs/sidecar-api.md`

---

## Sidecar Implementations

### Official Reference Sidecars

Located in `hamlib-audio-sidecar` repository
(https://github.com/jfrancis42/hamlib-audio-sidecar). Two Python entry
points plus a runtime CLI and a lifecycle script, all built on the
shared library `hamlib_sidecar_common.py`:

1. **`hamlib_sidecar_linux.py`** (Linux). Two user-side backends:
   - `--user-backend pulseaudio` (default) — virtual PulseAudio sinks
     `<prefix>-rx` and `<prefix>-tx`. With an audio-side rigctld port,
     it's passthrough demodulated audio. With an IQ-side port, the
     built-in SSB / AM / FM mono+stereo / CW demodulator + the full
     DSP suite (AGC, NR, NB, notch, APF, squelch) produces audio.
     Bidirectional (RX_AUDIO + TX_AUDIO).
   - `--user-backend pulseaudio-iq` — raw IQ presented as a stereo
     PulseAudio sink (L=I, R=Q). Bidirectional where the backend
     supports TX_IQ.

2. **`hamlib_sidecar_portable.py`** (any OS). Two user-side backends:
   - `--user-backend gnuradio` — four ZMQ endpoints: audio RX PUB,
     audio TX PULL, IQ RX PUB, IQ TX PULL. Cross-platform; no audio
     APIs needed.
   - `--user-backend soapysdr` — bypasses rigctld entirely and talks
     directly to a SoapySDR-supported radio (RTL-SDR, HackRF, LimeSDR,
     PlutoSDR, etc.). Useful for radios that don't have a Hamlib
     backend yet.

3. **`hamctl`** — runtime control CLI. Frequency, mode, every DSP
   level, AGC, S-meter (one-shot or live bargraph), PulseAudio loopback
   routing, favorites, band-aware `tune <khz>` with a JSON band plan
   that auto-applies mode/width/AGC. Talks to rigctld over its CAT
   port; works against any radio model.

4. **`hamlib.sh`** — lifecycle script that brings rigctld + sidecar +
   the PulseAudio loopback up as a unit. Has per-radio defaults for
   `--radio rtlsdr|kiwisdr|tci`.

### Platform-Specific Considerations

| Platform | Audio API | Status |
|----------|-----------|--------|
| **Linux** | PulseAudio / PipeWire | ✅ Working |
| **Windows** | WASAPI / VB-Audio Cable | 🔄 Planned |
| **macOS** | CoreAudio / BlackHole | 🔄 Planned |

**Cross-platform:** GNU Radio and ZMQ-based sidecars work on all platforms (no platform audio APIs needed).

---

## Lifecycle Management

### Manual Control

```bash
# Start rigctld with sidecar ports
rigctld -m <model> -C audio_port=4534 -C iq_port=4535 ...

# Start sidecars in separate terminals
python3 audio-sidecar.py --rigctld-port 4534
python3 iq-sidecar.py --rigctld-port 4535
```

### Automated (hamlib.sh)

The `hamlib-audio-sidecar` repo ships `hamlib.sh`:

```bash
./hamlib.sh start --radio {rtlsdr|kiwisdr|tci} \
                  [--host HOST] [--port PORT] [--stream audio|iq] \
                  [--rate HZ] [--sink PULSE_SINK_SUBSTRING] \
                  [--freq KHZ] [--mode MODE WIDTH_HZ]
./hamlib.sh stop          # kill processes, unload sinks
./hamlib.sh restart       # stop + start (same args)
./hamlib.sh status        # what's running, sinks, loopbacks
```

State lives under `$XDG_RUNTIME_DIR/hamlib-sdr/`. Per-radio defaults
fill in the right `-C` config tokens for rigctld so the user only has
to think about *what radio* and *where it lives*, not which port goes
where. For runtime tweaking (freq, mode, AGC, S-meter, audio routing,
favorites) use `hamctl` from the same repo.

---

## Testing

### Verify CAT Connection

```bash
echo 'f' | nc -w 1 localhost 4532    # Query frequency
echo 'm' | nc -w 1 localhost 4532    # Query mode
```

### Verify Audio Sidecar (Linux/PulseAudio)

```bash
# List sinks
pactl list sinks short | grep hamlib

# Record 3 seconds of RX audio
timeout 3 parec --device=hamlib-rx.monitor --rate=8000 \
    --channels=1 --format=s16le > /tmp/rx_check.raw

# Analyze
python3 -c "
import struct
d = open('/tmp/rx_check.raw','rb').read()
s = struct.unpack('<' + str(len(d)//2) + 'h', d)
peak = max(abs(x) for x in s) if s else 0
nz = sum(1 for x in s if x != 0)
print(f'samples={len(s)}, peak={peak}, nonzero={nz}')
"
```

Expect 24000 samples, peak > 100, nonzero fraction > 95%

### Verify IQ via portable sidecar (ZMQ)

```bash
# hamlib_sidecar_portable.py --user-backend gnuradio publishes RX IQ
# as complex64 on tcp://*:5552.
python3 -c "
import zmq, time
s = zmq.Context.instance().socket(zmq.SUB)
s.setsockopt(zmq.SUBSCRIBE, b'')
s.setsockopt(zmq.RCVTIMEO, 2000)
s.connect('tcp://localhost:5552')
end = time.time() + 3
msgs = samples = 0
while time.time() < end:
    try: m = s.recv()
    except zmq.Again: continue
    msgs += 1
    samples += len(m) // 8   # complex64 = 8 bytes
print(f'{msgs} ZMQ messages, {samples} complex samples in 3 s')
"
```

Effective rate should match the rigctld `-C iq_rate=N` setting.

### Ultimate Test

**Call CQ with JS8Call.** If you get replies, the audio pipeline works end-to-end.

---

## Backend Support

### Currently in this PR

| Backend | Audio | IQ | Bidirectional | Status |
|---------|-------|----|--------------:|--------|
| **TCI 2.0** (ExpertSDR3) | ✅ | ✅ | TX audio only | Working (minor latent audio issues noted) |

### Working but held back from this PR

The same sidecar protocol drives two additional backends that have been
written end-to-end and tested with live signals, but are not part of
this PR — they're waiting for the sidecar/protocol design to settle:

| Backend | Audio | IQ | Status |
|---------|-------|----|--------|
| **KiwiSDR** (model 102, WebSocket) | ✅ | ✅ (RX) | Working — verified HF AM/SSB/CW + IQ mode |
| **RTL-SDR** (model 202, librtlsdr) | — | ✅ (RX) | Working — verified WFM stereo, airband AM, NBFM |

### Other plausible backends

| Backend | Audio | IQ | Notes |
|---------|-------|----|-------|
| **SoapySDR** | ❌ | ✅ | Already covered by `hamlib_sidecar_portable.py --user-backend soapysdr` (no Hamlib backend needed) |
| **FlexRadio** | ✅ | ✅ | VITA-49, buffer pacing, spectrum |
| **Perseus** | ❌ | ✅ | IQ-native, USB |
| **Airspy HF+** | ❌ | ✅ | IQ-native, USB |
| **SDRPlay** | ❌ | ✅ | IQ-native, dual tuner (RSPduo) |
| **ICOM IC-7300/IC-705** | ✅ | ❌ | Native radio, audio only |

---

## Protocol Extensions

### For Backend Authors

When implementing a backend with unique requirements:

1. **Check existing types** - Many needs already covered (types 0-24)
2. **Use reserved fields** - 32 bytes (words 8-15) available for metadata
3. **Define subtypes** - For METADATA/CAPABILITY/SPECTRUM, define subtypes in backend code
4. **Document** - Add to backend README and `docs/sidecar-api.md`
5. **Vendor extensions** - Types 1000-1999 for proprietary features

**Example:** KiwiSDR GPS timestamps:
```c
// In kiwisdr.c
#define KIWI_META_GPS_TIMESTAMP 1

// Emit timestamp
uint32_t hdr[16] = {0};
hdr[5] = KIWI_META_GPS_TIMESTAMP;   // length = subtype
hdr[6] = SIDECAR_STREAM_METADATA;   // type 22
hdr[8] = (uint32_t)(timestamp_ns & 0xFFFFFFFF);
hdr[9] = (uint32_t)(timestamp_ns >> 32);
send(fd, hdr, 64, MSG_NOSIGNAL);
```

**Complete extension guide:** `docs/SIDECAR-EXTENSIBILITY-ANALYSIS.md`

---

## References

### Documentation
- **API Reference:** `docs/sidecar-api.md`
- **Backend Integration:** `docs/sidecar-api.md` (section: Backend Integration Guide)
- **Protocol Spec:** `hamlib-audio-sidecar/PROTOCOL.md`
- **Extensibility:** `docs/SIDECAR-EXTENSIBILITY-ANALYSIS.md`

### Code
- **API Header:** `include/hamlib/sidecar.h`
- **Implementation:** `src/sidecar.c`
- **Example Backend:** `rigs/dummy/tci2.c`
- **Sidecar Repo:** https://github.com/jfrancis42/hamlib-audio-sidecar

### Backend-Specific READMEs
- **TCI 2.0:** `rigs/dummy/README-TCI-2.0.md`
- *(More backends as they're implemented)*

---

## License

LGPL 2.1+ (same as Hamlib)
