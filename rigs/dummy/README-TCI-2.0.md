# TCI 2.0 Backend Audio and IQ Support

**Backend Model:** 12 (TCI 2.0)  
**Protocol:** TCI 2.0 WebSocket  
**Target:** ExpertSDR3, Apache Labs ANAN with TCI firmware

---

## Quick Start

Easiest path (Linux): use the lifecycle script from the sidecar repo
(https://github.com/jfrancis42/hamlib-audio-sidecar):

```bash
# Audio path
./hamlib.sh start --radio tci --host 127.0.0.1 --port 50001 \
                  --stream audio --sink <pulse-sink-substring>

# IQ path (RX-only on TCI)
./hamlib.sh start --radio tci --host 127.0.0.1 --port 50001 \
                  --stream iq --rate 192000
```

By hand:

```bash
# Start rigctld with both audio and IQ sidechannels enabled
rigctld -m 12 -r localhost:50001 -t 4532 \
        -C audio_port=4534 \
        -C iq_port=4535 -C iq_rate=192000

# Linux PulseAudio sidecar (audio path)
python3 hamlib_sidecar_linux.py --rigctld-port 4534 --output-rate 48000

# Portable sidecar (GNU Radio ZMQ; works on any OS)
python3 hamlib_sidecar_portable.py --rigctld-port 4534 \
                                   --user-backend gnuradio
```

Configure your ham-radio software:
- **Audio input (RX):** `hamlib-rx.monitor` (PulseAudio sidecar)
- **Audio output (TX):** `hamlib-tx` (PulseAudio sidecar)
- **CAT:** Hamlib `rigctld` at `localhost:4532`
- **IQ (ZMQ via portable sidecar):** `tcp://localhost:5552` (complex float32)

---

## TCI-Specific Architecture

```
       ┌──────────────┐    TCI 2.0      ┌─────────────┐
       │  ExpertSDR3  │◀──WebSocket────▶│   rigctld   │◀── CAT (JS8Call, etc.) :4532
       │  (port 50001)│                 │   (model 12)│
       └──────────────┘                 └──┬───────┬──┘
                                           │       │
                           audio :4534 ────┘       └──── iq :4535
                                           │              │
                                           ▼              ▼
                        ┌────────────────────────────────────────────┐
                        │       Sidecar (Python, separate repo)      │
                        │                                            │
                        │   hamlib_sidecar_linux.py    PulseAudio    │
                        │                              (audio or     │
                        │                               IQ-stereo)   │
                        │   hamlib_sidecar_portable.py GNU Radio     │
                        │                              ZMQ or        │
                        │                              SoapySDR      │
                        └─────────────┬──────────────────┬───────────┘
                                      │                  │
                              PulseAudio sinks       ZMQ sockets
                              (hamlib-rx,            (audio + IQ
                               hamlib-tx)             RX / TX)
                                      │                  │
                                      ▼                  ▼
                              JS8Call, fldigi,     GNU Radio,
                              WSJT-X, gqrx         SDR apps
```

**rigctld** owns the one TCI WebSocket connection to ExpertSDR3. Audio and IQ are proxied via **Hamlib's generic sidecar protocol** over independent TCP ports (4534 and 4535) to an external Python sidecar process.

**See also:** `README.audio-sidecar.md` in Hamlib root for generic sidecar architecture and protocol specification.

---

## TCI 2.0 Protocol Specifics

### What TCI 2.0 Provides

- **Audio (bidirectional):** 8 kHz mono int16 (RX + TX)
- **IQ (RX-only):** 48/96/192/384 kHz float32 complex
- **CAT:** Full rig control via WebSocket text commands

### What TCI 2.0 Does NOT Provide

- **TX IQ:** TCI 2.0 spec defines `IQ_STREAM` (type 0) as **unidirectional** RX-only. There is no `TX_IQ_STREAM` or mechanism for a client to push IQ samples back to the radio for transmission. This is a protocol limitation, not a Hamlib choice.

TX of arbitrary baseband signals remains possible via the audio path (`TX_AUDIO_STREAM`), which ExpertSDR3 accepts at 8 kHz mono for HF digital modes.

Adding TX IQ outside the spec would break interop with non-Expert-Electronics TCI implementations (e.g. Apache Labs ANAN with TCI firmware).

### TCI Stream Type Mapping

The TCI 2.0 spec defines these stream types (in its `StreamType` enum):

| TCI StreamType | Hamlib stream_type | Direction | Description |
|----------------|-------------------|-----------|-------------|
| 0 (IQ_STREAM) | 0 (STREAM_IQ) | RX-only | I/Q samples |
| 1 (RX_AUDIO_STREAM) | 1 (STREAM_RX_AUDIO) | RX | Demodulated audio |
| 2 (TX_AUDIO_STREAM) | 2 (STREAM_TX_AUDIO) | TX | Audio to modulate |
| 3 (TX_CHRONO) | 3 (STREAM_TX_CHRONO) | RX | TX pacing pulse |

Hamlib extends this with types 4-24 for generic sidecar use (PTT state, control frames, future extensions). See `README.audio-sidecar.md` for full list.

---

## Configuration Parameters

TCI 2.0 backend config options:

```bash
rigctld -m 12 -r HOST:50001 -t 4532 \
    -C trx=0 \                       # TRX index (0-based, for multi-RX rigs)
    -C txsource=default \            # or 'mic', 'vac' (overridden by PTT type)
    -C digl_offset=0 \               # DIGL freq offset, Hz (0..4000)
    -C digu_offset=0 \               # DIGU freq offset, Hz (0..4000)
    -C audio_port=4534 \             # Audio sidecar port (0=disabled)
    -C iq_port=4535 \                # IQ sidecar port (0=disabled)
    -C iq_rate=192000                # IQ sample rate (48k/96k/192k/384k)
```

### TCI-Specific Options

- **`trx`** — TRX index (0-based). For multi-receiver rigs (e.g., dual VFO).
- **`txsource`** — TX audio source sent in `TRX:` command:
  - `default` — Let ExpertSDR3 decide
  - `mic` — Microphone input
  - `vac` — VAC (Virtual Audio Cable, for data modes)
- **`digl_offset` / `digu_offset`** — Frequency offset for DIGL/DIGU modes (Hz, 0-4000). Hamlib reports VFO freq ± offset to match modem expectation.

### Generic Sidecar Options

- **`audio_port`** — TCP port for audio sidecar (0=disabled). Auto-enables TCI `AUDIO_START`.
- **`iq_port`** — TCP port for IQ sidecar (0=disabled). Auto-enables TCI `IQ_START`.
- **`iq_rate`** — IQ sample rate (Hz) requested from TCI. ExpertSDR3 supports 48000 / 96000 / 192000 / 384000. Default 192000.

---

## TCI-Compatible Sidecars

All sidecars from the `hamlib-audio-sidecar` repo work with the TCI 2.0
backend. The Python codebase is the same one used for KiwiSDR and
RTL-SDR — only the rigctld-side config changes.

### `hamlib_sidecar_linux.py` (Linux, two user-side modes)

- **`--user-backend pulseaudio`** (default) — demodulated audio in/out via
  virtual PulseAudio sinks `hamlib-rx` and `hamlib-tx`. On TCI that's
  bidirectional 8 kHz mono direct from ExpertSDR3. Works with JS8Call,
  fldigi, WSJT-X.
- **`--user-backend pulseaudio-iq`** (connect to `:4535`) — raw IQ presented
  as a stereo PulseAudio sink with L=I, R=Q. **RX-only on TCI** (the spec
  doesn't define TX IQ; TCI IQ TX frames are discarded). For software that
  expects to pull IQ from a soundcard (PowerSDR, HDSDR, fldigi IQ mode).

### `hamlib_sidecar_portable.py` (any OS)

- **`--user-backend gnuradio`** — RX audio on `tcp://*:5550`, TX audio on
  `tcp://*:5551`, RX IQ on `tcp://*:5552`, TX IQ on `tcp://*:5553`. All
  ZMQ PUB/PULL, complex64 or float32 mono as appropriate. TX IQ is sent
  upstream but ignored by the TCI backend.
- **`--user-backend soapysdr`** — bypasses rigctld entirely and talks to a
  SoapySDR device directly. Not used with TCI; mentioned for completeness.

### TX Gain Requirement (IMPORTANT)

**ExpertSDR3 silently drops TX audio below an internal threshold.** Most
modems (JS8Call, fldigi) output around -20 dBFS. The sidecar's
`SmartSidecar` has a peak-tracking AGC on the TX path that brings audio
up to ExpertSDR3's expected level; if you need to tweak it, see the
`apply_agc` block in `hamlib_sidecar_common.py` or the AGC discussion in
the sidecar repo's `DEVELOPERS.md`.

---

## Lifecycle Management

### Manual Start

```bash
# Terminal 1: rigctld
rigctld -m 12 -r localhost:50001 -t 4532 \
        -C audio_port=4534 -C iq_port=4535

# Terminal 2: Linux sidecar — PulseAudio audio backend
python3 hamlib_sidecar_linux.py --rigctld-port 4534 --output-rate 48000

# Terminal 3 (optional): a second sidecar for IQ as stereo soundcard
python3 hamlib_sidecar_linux.py --rigctld-port 4535 \
                                --user-backend pulseaudio-iq \
                                --iq-rate 192000
```

### Automated (hamlib.sh)

The `hamlib-audio-sidecar` repo ships `hamlib.sh`:

```bash
./hamlib.sh start --radio tci --host 127.0.0.1 --port 50001 \
                  --stream audio --sink <pulse-sink-substring>
./hamlib.sh stop                  # kill processes, unload sinks
./hamlib.sh restart [opts...]     # stop + start
./hamlib.sh status                # processes, sinks, loopbacks
```

State is kept under `$XDG_RUNTIME_DIR/hamlib-sdr/`. The script picks
sensible defaults for `-C` config tokens per radio (so the `--port`
above is the TCI port, not the sidechannel — those are filled in
automatically). For runtime tweaking (mode, AGC, S-meter, audio
routing) reach for `hamctl` in the same repo.

---

## Testing

### CAT Control

```bash
# Query frequency
echo 'f' | nc -w 1 localhost 4532

# Query mode
echo 'm' | nc -w 1 localhost 4532

# Set frequency to 14.074 MHz
echo 'F 14074000' | nc -w 1 localhost 4532
```

### Audio Sidecar (PulseAudio)

```bash
# List PulseAudio sinks
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
# Expect 24000 samples, peak > 100, nonzero > 95%
```

### IQ via portable sidecar (ZMQ)

```bash
# Start hamlib_sidecar_portable.py --user-backend gnuradio first.
# RX IQ is published on tcp://*:5552 (complex64).
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
print(f'effective rate: {samples/3:.0f} Hz')
"
# Expect effective rate ≈ 192000 Hz when iq_rate=192000
```

### Quick poke with hamctl

```bash
./hamctl show                  # current freq, mode, DSP, audio routing
./hamctl smeter live           # realtime S-meter bargraph (q to quit)
./hamctl freq 14074            # tune to 14.074 MHz
./hamctl mode USB 2400         # mode + filter width
```

### Ultimate Test

**Call CQ with JS8Call** on 14.074 MHz. If you get replies, the audio pipeline works end-to-end.

---

## Verified End-to-End

- **JS8Call** (14.079 MHz, 40 m): CQ got replies. On-air decode confirmed by remote receivers.
- **1 kHz tone TX** (pacat via PulseAudio sidecar): clean carrier at 14.0790 MHz, -27 to -33 dBm sustained. Zero silence frames mid-transmission.
- **RX audio** (parec from `hamlib-rx.monitor`): 8000 samples/sec, 98% nonzero, peak ~1100.
- **IQ stream** (`hamlib_sidecar_portable.py --user-backend gnuradio`): effective rate matches configured `iq_rate`.

---

## JS8Call Audio Buffer Setting (Recommended)

By default JS8Call lets Qt pick the PulseAudio buffer size (~2 s on Linux). Add this to `~/.config/JS8Call.ini`:

```ini
[Tune]
Audio\OutputBufferMs=200
```

Restart JS8Call. This shrinks the PA buffer to 200 ms (deterministic instead of Qt-default). It does NOT shrink the observed ~2 s TX pre-roll — that is intrinsic to JS8Call's slot alignment scheduler, not the PA buffer — but it does make the PA-side latency predictable.

---

## TCI Protocol Normalization (Implementation Note)

The tci2.c backend normalizes TCI responses by uppercasing command names before the colon:

```c
/* In ws_recv_frame() lines 593-599:
   Normalize: uppercase the command keyword (before the first ':') so that
   sscanf format strings and prefix comparisons work regardless of whether
   the server sends "VFO:" or "vfo:". */
for (size_t i = 0; i < plen && buf[i] != ':' && buf[i] != ';'; i++)
{
    buf[i] = (char)toupper((unsigned char)buf[i]);
}
```

**Implication:** All sscanf patterns in tci2.c use uppercase for command names, even though the TCI protocol spec uses lowercase. If you modify the TCI parsing code, remember this normalization step.

---

## TCI Commands Used by Backend

The tci2.c backend uses these TCI commands (not exhaustive):

- **Frequency:** `VFO:trx,channel;` → `VFO:trx,channel,frequency;`
- **Mode:** `MODULATION:trx;` → `MODULATION:trx,mode;`
- **PTT:** `TRX:trx,true[,source];` / `TRX:trx,false;`
  - Source can be `,Mic` or `,Vac` for data modes (controlled by `txsource` config)
- **Audio control:** `AUDIO_START:trx;` / `AUDIO_STOP:trx;`
- **IQ control:** `IQ_START:trx;` / `IQ_STOP:trx;`
- **Audio format:** `AUDIO_SAMPLERATE:trx,8000;` (always 8 kHz for TCI audio)
- **IQ format:** `IQ_SAMPLERATE:trx,<rate>;` (from `iq_rate` config)

---

## Platform Status

- **Linux** — ✅ working. `hamlib_sidecar_linux.py` (PulseAudio audio +
  IQ-stereo backends) plus `hamlib_sidecar_portable.py` (ZMQ + SoapySDR).
- **Windows** — 🔄 native PulseAudio replacement is planned (WASAPI or
  VB-Audio Cable, planning notes in the sidecar repo's `windows.md`).
  `hamlib_sidecar_portable.py` works as-is on Windows for the ZMQ /
  SoapySDR paths.
- **macOS** — 🔄 native PulseAudio replacement is planned (CoreAudio or
  BlackHole, planning notes in `osx.md`). `hamlib_sidecar_portable.py`
  works as-is on macOS for the ZMQ / SoapySDR paths.

---

## References

### Generic Sidecar Documentation
- **Architecture overview (Hamlib side):** `README.audio-sidecar.md` (Hamlib root)
- **API reference:** `docs/sidecar-api.md`
- **Protocol spec (canonical):** `PROTOCOL.md` in the sidecar repo
- **User guide:** `USERS.md` in the sidecar repo (covers `hamlib.sh` /
  `hamctl`, JS8Call / fldigi / WSJT-X / gqrx setup, troubleshooting)
- **Developer guide:** `DEVELOPERS.md` in the sidecar repo (writing a
  new sidecar in any language, or extending the Python codebase)
- **Linux internals:** `linux.md` in the sidecar repo
- **Sidecar repo:** https://github.com/jfrancis42/hamlib-audio-sidecar

### TCI-Specific
- **TCI Protocol PDF:** `TCI_Protocol.pdf` in Hamlib root
- **Working C++ TCI reference:** https://github.com/maksimus1210/TCI
- **Working Python TCI reference:** eesdr-tci library (pip install eesdr-tci)

### Hamlib Code
- **Backend implementation:** `rigs/dummy/tci2.c`
- **Sidecar API:** `include/hamlib/sidecar.h`, `src/sidecar.c`

---

## License

LGPL 2.1+ (same as Hamlib)
