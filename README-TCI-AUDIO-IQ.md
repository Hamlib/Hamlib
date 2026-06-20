# TCI 2.0 Audio and IQ Support via External Sidecars

Full audio (bidirectional RX/TX) and IQ (RX-only) streaming for the TCI 2.0 backend.

## Quick Start

```bash
# Start rigctld with both audio and IQ sidechannels enabled
rigctld -m 12 -r localhost:50001 -t 4532 \
        -C audio_port=4534 \
        -C iq_port=4535 -C iq_rate=192000

# In separate terminals:

# Audio sidecar (pick one style):
python3 tci-audio-soundcard-sidecar.py \
    --rigctld-host localhost --rigctld-port 4534 \
    --name tci --tx-gain-db 20 --rx-gain-db 0
# (or use tci-audio-gr-sidecar.py for GNU Radio instead)

# IQ sidecar (optional; independent of audio)
python3 tci-iq-sidecar.py \
    --rigctld-host localhost --rigctld-port 4535 \
    --zmq-bind 'tcp://*:5555'
```

Configure your ham-radio software:
- **With `tci-audio-soundcard-sidecar.py` (PulseAudio)**:
  - Audio input (RX): `tci-rx.monitor`
  - Audio output (TX): `tci-tx`
- **With `tci-audio-gr-sidecar.py` (GNU Radio)**:
  - RX: `zmq_sub_source` on `tcp://localhost:5557` (float32 mono 8 kHz)
  - TX: `zmq_push_sink` on `tcp://localhost:5558` (float32 mono 8 kHz)
- **CAT**: Hamlib `rigctld` at `localhost:4532`
- **IQ**: `zmq_sub_source` on `tcp://localhost:5555` (complex float32, rate set by `-C iq_rate=`)

## Architecture

```
       ┌──────────────┐    TCI         ┌─────────────┐
       │  ExpertSDR3  │◀──WebSocket───▶│   rigctld   │◀── CAT (JS8Call, etc.) :4532
       │  (port 50001)│                │             │
       └──────────────┘                └──┬───────┬──┘
                                          │       │
                          audio :4534 ────┘       └──── iq :4535
                                          │              │
                                          ▼              ▼
                              ┌──────────────────┐  ┌──────────────────┐
                              │  audio sidecar   │  │ tci-iq-sidecar.py│
                              │ (soundcard or GR)│  │                  │
                              └────┬────────┬────┘  └─────────┬────────┘
                       tci-rx ◀────┘        └───▶ tci-tx      │
                       (sink)                     (sink)      │ ZMQ PUB :5555
                          ▲                          ▲        │ (complex float32)
                          │                          │        │
                          │      modem reads RX,     │        │
                          └─────  writes TX  ────────┘        ▼
                                                       GR flowgraph,
                                                       tci-iq-viewer.py,
                                                       any ZMQ consumer
```

**rigctld** owns the one TCI WebSocket connection to ExpertSDR3. Audio and IQ are proxied as **length-framed binary TCI frames** over independent TCP sidechannels (4534 and 4535) to external Python sidecar processes.

## Why External Sidecars?

- **No new Hamlib dependencies** — rigctld stays pure C; no libpulse/coreaudio/wasapi.
- **Platform flexibility** — Linux, Windows, and macOS sidecars use completely different audio APIs without touching Hamlib code.
- **Process isolation** — audio/IQ bugs don't crash rigctld.
- **Fast iteration** — audio pipeline changes are Python edits + restart, not Hamlib rebuild.

## Wire Protocol (rigctld ↔ sidecar)

Pure binary, length-framed TCI frames in both directions:

```
64-byte header (16 little-endian uint32 words) + 0..N payload bytes
```

| Offset | Field       | Audio/IQ frames         | Control frames |
|--------|-------------|-------------------------|----------------|
| 0      | receiver    | trx index               | trx index      |
| 4      | sample_rate | Hz                      | 0              |
| 8      | format      | 0=int16 1=int24 2=int32 3=float32 | 0 |
| 20     | length      | samples in payload      | control value  |
| 24     | stream_type | 0=IQ 1=RX_AUDIO 2=TX_AUDIO | 3=TX_CHRONO 4=PTT_STATE |
| 28     | channels    | 1 (audio) or 2 (IQ)     | 1              |
| 32..63 | reserved    | zero-filled             | zero-filled    |

**stream_type dispatch:**
- `STREAM_IQ (0)` — receiver IQ (rigctld → IQ sidecar, **RX-only — TCI 2.0 does not define TX IQ**)
- `STREAM_RX_AUDIO (1)` — receiver audio (rigctld → audio sidecar)
- `STREAM_TX_AUDIO (2)` — transmit audio (audio sidecar → rigctld)
- `STREAM_TX_CHRONO (3)` — hamlib-internal: radio requests N samples (rigctld → audio sidecar)
- `STREAM_PTT_STATE (4)` — hamlib-internal: PTT state change (rigctld → audio sidecar)

Values 0..2 match TCI's `StreamType` enum; 3+ are hamlib-internal.

**Why binary-only?** Audio/IQ payloads contain arbitrary bytes (including 0x0A). Text framing corrupts streams.

## Config Parameters

```bash
rigctld -m 12 -r HOST:50001 -t 4532 \
    -C trx=0 \                       # TRX index (0-based, for multi-RX rigs)
    -C txsource=default \            # or 'mic', 'vac' (overridden by PTT type)
    -C digl_offset=0 \               # DIGL freq offset, Hz (0..4000)
    -C digu_offset=0 \               # DIGU freq offset, Hz (0..4000)
    -C audio_port=4534 \             # audio sidechannel port (0=disabled)
    -C iq_port=4535 \                # IQ sidechannel port (0=disabled)
    -C iq_rate=192000                # IQ sample rate (48k/96k/192k/384k)
```

- **`audio_port`** — TCP port for the audio sidechannel. Auto-enables TCI audio (`AUDIO_START`).
- **`iq_port`** — TCP port for the IQ sidechannel. Auto-enables TCI IQ (`IQ_START`). **Independent of `audio_port`** — both can be enabled together.
- **`iq_rate`** — sample rate (Hz) requested for the TCI IQ stream when `iq_port` is enabled. ExpertSDR3 supports 48000 / 96000 / 192000 / 384000. Default 192000.

## Sidecar Implementations

**Three sidecars ship in `hamlib-tci-sidecar` repo:**

### Audio Sidecars (choose one; both connect to `:4534`)

1. **`tci-audio-soundcard-sidecar.py`** — PulseAudio/PipeWire (Linux). RX/TX audio as null sinks (`tci-rx`, `tci-tx`) for JS8Call, fldigi, WSJT-X, etc. Bidirectional.

   ```bash
   python3 tci-audio-soundcard-sidecar.py \
       --rigctld-host localhost --rigctld-port 4534 \
       --name tci --tx-gain-db 20 --rx-gain-db 0
   ```

   - **TX gain** (default +20 dB) — ExpertSDR3 silently drops TX audio below an internal threshold. JS8Call and other modems output ~-20 dBFS. +20 dB clipping brings it up to ExpertSDR3's expected level.
   - **RX gain** (default 0 dB) — for symmetry. Use small positive values to boost RX into modems that want louder input.

2. **`tci-audio-gr-sidecar.py`** — GNU Radio audio bridge. RX audio published as ZMQ PUB (float32 mono 8 kHz, `:5557`); TX audio accepted on ZMQ PULL (float32 mono 8 kHz, `:5558`). Alternative to the PulseAudio sidecar. Bidirectional.

   ```bash
   python3 tci-audio-gr-sidecar.py \
       --rigctld-host localhost --rigctld-port 4534 \
       --zmq-rx-bind 'tcp://*:5557' \
       --zmq-tx-bind 'tcp://*:5558' \
       --tx-gain-db 20 --rx-gain-db 0
   ```

   - GR flowgraphs: `zmq_sub_source` (RX) and `zmq_push_sink` (TX), item type `float`, vec length 1, sample rate 8000.
   - Same TX/RX gain semantics as the PulseAudio sidecar.

### IQ Sidecar (connects to `:4535`, independent of audio)

3. **`tci-iq-sidecar.py`** — IQ bridge. Receiver IQ stream published as ZMQ PUB (complex float32, `:5555`). **RX-only** (TCI does not define TX IQ). For GNU Radio and other ZMQ-aware SDR consumers.

   ```bash
   python3 tci-iq-sidecar.py \
       --rigctld-host localhost --rigctld-port 4535 \
       --zmq-bind 'tcp://*:5555'
   ```

   - GR: `zmq_sub_source`, Address `tcp://HOST:5555`, Type `complex float`.
   - Sample rate set by rigctld's `-C iq_rate=` (default 192000).
   - ZMQ PUB is lossy under backpressure — drops old samples rather than stalling the radio.

### GR-side Tools (demonstrate the sidecars)

- **`tci-iq-viewer.py`** — Qt FFT + waterfall against the IQ sidecar. Polls rigctld for dial frequency every 500 ms. Left-click to retune.
- **`tci-audio-gr-tester.py`** — Qt FFT + waveform of RX audio plus a 1 kHz tone generator gated by a PTT button. Demonstrates the full GR audio integration.

Both require GNU Radio 3.10+ with `gr-zeromq` and `qtgui`, plus PyQt5.

## Surprising Detail: TCI 2.0 IQ is RX-Only

The TCI 2.0 spec defines `IQ_STREAM` (stream_type=0) as **unidirectional** from radio to client. There is no `TX_IQ_STREAM` or any mechanism for a client to push IQ samples back to the radio for transmission. This is a protocol limitation, not a Hamlib choice.

TX of arbitrary baseband signals remains possible via the audio path (`TX_AUDIO_STREAM`), which ExpertSDR3 accepts at 8 kHz mono for HF digital modes.

Adding TX IQ outside the spec would break interop with non-Expert-Electronics TCI implementations (e.g. Apache Labs ANAN with TCI firmware).

## Lifecycle Management

The `tci.sh` script in `hamlib-tci-sidecar` manages rigctld + sidecars as a unit:

```bash
./tci.sh start     # launch rigctld + enabled sidecars (idempotent)
./tci.sh stop      # tear down everything, unload PulseAudio sinks
./tci.sh restart   # stop + start
./tci.sh status    # what's running, sinks, listening ports, last log lines
./tci.sh log       # tail -F the audio sidecar log
./tci.sh iqlog     # tail -F the IQ sidecar log
```

Edit the variables at the top of `tci.sh` to point at your rigctld binary and Hamlib library locations, and to enable/disable audio and IQ sidecars.

## Verified End-to-End

- **JS8Call** (14.079 MHz, 40 m): CQ got replies. On-air decode confirmed by remote receivers.
- **1 kHz tone TX** (pacat via PulseAudio sidecar): clean carrier at 14.0790 MHz, -27 to -33 dBm sustained. Zero silence frames mid-transmission.
- **RX audio** (parec from `tci-rx.monitor`): 8000 samples/sec, 98% nonzero, peak ~1100.
- **IQ stream** (via `tci-iq-sidecar.py`): ~280 ZMQ messages, ~580k complex samples in 3 s. Effective rate matches configured `iq_rate`.

## Testing

```bash
# CAT
echo 'f' | nc -w 1 localhost 4532    # dial freq
echo 'm' | nc -w 1 localhost 4532    # mode + width

# Audio sidecar running: RX audio flows into tci-rx.monitor
timeout 3 parec --device=tci-rx.monitor --rate=8000 --channels=1 \
    --format=s16le > /tmp/rx_check.raw
python3 -c "
import struct
d = open('/tmp/rx_check.raw','rb').read()
s = struct.unpack('<' + str(len(d)//2) + 'h', d)
peak = max(abs(x) for x in s) if s else 0
nz = sum(1 for x in s if x != 0)
print(f'samples={len(s)}, peak={peak}, nonzero={nz}')
"
# Expect 24000 samples, peak well above 100, fraction nonzero > 95%

# IQ sidecar running: IQ stream reachable on ZMQ
python3 -c "
import zmq, time, numpy as np
s = zmq.Context.instance().socket(zmq.SUB)
s.setsockopt(zmq.SUBSCRIBE, b'')
s.setsockopt(zmq.RCVTIMEO, 2000)
s.connect('tcp://localhost:5555')
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
# Expect ~280 messages, ~580k samples, effective rate ≈ iq_rate (default 192000)

# GR IQ viewer
python3 tci-iq-viewer.py

# GR audio tester (only with tci-audio-gr-sidecar)
python3 tci-audio-gr-tester.py
```

Ultimate test: **call CQ with JS8Call**. If you get replies, the audio pipeline is working on-air.

## JS8Call Audio Buffer Setting (Recommended)

By default JS8Call lets Qt pick the PulseAudio buffer size (~2 s on Linux). Add this to `~/.config/JS8Call.ini`:

```ini
[Tune]
Audio\OutputBufferMs=200
```

Restart JS8Call. This shrinks the PA buffer to 200 ms (deterministic instead of Qt-default). It does NOT shrink the observed ~2 s TX pre-roll — that is intrinsic to JS8Call's slot alignment scheduler, not the PA buffer — but it does make the PA-side latency predictable.

## Platform Status

- **Linux** — working (PulseAudio / PipeWire audio sidecar, ZMQ-based GR audio sidecar, IQ sidecar).
- **Windows** — planned (same wire protocol, different audio APIs: WASAPI loopback or VB-Audio Cable). GR audio sidecar and IQ sidecar already run on Windows as-is (ZMQ + numpy, no platform-specific audio).
- **macOS** — planned (same wire protocol, BlackHole or CoreAudio aggregate device). GR audio sidecar and IQ sidecar already run on macOS as-is.

## References

- **TCI Protocol PDF:** `TCI_Protocol.pdf` in this repo
- **Sidecar repo:** https://github.com/jfrancis42/hamlib-tci-sidecar (includes `PROTOCOL.md`, `README.md`, `tci.sh`)
- **Working C++ TCI reference:** https://github.com/maksimus1210/TCI
- **Working Python TCI reference:** eesdr-tci library (pip install eesdr-tci)

## License

Same as Hamlib (LGPL 2.1+).
