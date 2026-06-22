# TCI 2.0 Audio and IQ Support via External Sidecars

Full audio (bidirectional RX/TX) and IQ (RX-only) streaming for the TCI 2.0 backend.

## Quick Start

Easiest path (Linux, audio path): use the lifecycle script from the
sidecar repo (https://github.com/jfrancis42/hamlib-audio-sidecar) — it
starts rigctld, the sidecar, and the PulseAudio loopback in one command.

```bash
# Audio path: bidirectional 8 kHz mono RX/TX through virtual PulseAudio sinks
./hamlib.sh start --radio tci --host 127.0.0.1 --port 50001 \
                  --stream audio --sink <your-output-substring>

# IQ path: TCI RX-only IQ stream
./hamlib.sh start --radio tci --host 127.0.0.1 --port 50001 \
                  --stream iq --rate 192000 --sink <your-output-substring>
```

Configure your ham-radio software:
- **Audio input (RX):** `hamlib-rx.monitor` (PulseAudio)
- **Audio output (TX):** `hamlib-tx` (PulseAudio)
- **CAT:** Hamlib `rigctld` at `localhost:4532`

By hand, without the lifecycle script:

```bash
# Start rigctld with both audio and IQ sidechannels enabled
rigctld -m 12 -r localhost:50001 -t 4532 \
        -C audio_port=4534 \
        -C iq_port=4535 -C iq_rate=192000

# Linux sidecar — virtual PulseAudio audio devices
python3 hamlib_sidecar_linux.py --rigctld-port 4534 --output-rate 48000

# Or, for IQ on a separate sidecar process
python3 hamlib_sidecar_linux.py --rigctld-port 4535 \
                                --user-backend pulseaudio-iq \
                                --iq-rate 192000
```

For GNU Radio or SoapySDR (Linux / macOS / Windows):

```bash
python3 hamlib_sidecar_portable.py --rigctld-port 4534 \
                                   --user-backend gnuradio
# ZMQ endpoints: tcp://*:5550 RX audio, *:5551 TX audio,
#                *:5552 RX IQ, *:5553 TX IQ
```

## Architecture

```
       ┌──────────────┐    TCI         ┌─────────────┐
       │  ExpertSDR3  │◀──WebSocket───▶│   rigctld   │◀── CAT (JS8Call, etc.) :4532
       │  (port 50001)│                │  (model 12) │
       └──────────────┘                └──┬───────┬──┘
                                          │       │
                          audio :4534 ────┘       └──── iq :4535
                                          │              │
                                          ▼              ▼
                          ┌──────────────────────────────────────────┐
                          │       Sidecar (Python, separate repo)    │
                          │                                          │
                          │   hamlib_sidecar_linux.py   (PulseAudio  │
                          │                              backends)   │
                          │   hamlib_sidecar_portable.py  (ZMQ /     │
                          │                              SoapySDR)   │
                          └──────────────┬──────────────────┬────────┘
                                         │                  │
                                  PulseAudio sinks      ZMQ sockets
                                  (hamlib-rx,           (audio + IQ
                                   hamlib-tx)            RX / TX)
                                         │                  │
                                         ▼                  ▼
                                 JS8Call, fldigi,    GNU Radio,
                                 WSJT-X, gqrx        SDR apps
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

The Python sidecars live in `hamlib-audio-sidecar`
(https://github.com/jfrancis42/hamlib-audio-sidecar). The repo ships
two sidecar entry points, one runtime CLI, and one lifecycle script,
all built on a shared library:

### `hamlib_sidecar_linux.py` (connects to `:4534` for audio, `:4535` for IQ)

Linux-only. Two user-side backends, selected with `--user-backend`:

- **`pulseaudio`** (default) — demodulated audio in/out via virtual
  PulseAudio null sinks `hamlib-rx` and `hamlib-tx`. With TCI, that's
  bidirectional 8 kHz mono. With an IQ-only backend (KiwiSDR, RTL-SDR)
  the sidecar's built-in demodulator (SSB / AM / FM mono+stereo / CW)
  produces audio. Compatible with JS8Call, fldigi, WSJT-X, etc.
- **`pulseaudio-iq`** — raw IQ as a stereo PulseAudio sink with L=I, R=Q.
  For software that expects to pull IQ from a soundcard (fldigi IQ mode,
  gqrx, HDSDR, PowerSDR).

```bash
# Audio
python3 hamlib_sidecar_linux.py --rigctld-port 4534 --output-rate 48000

# IQ as stereo soundcard
python3 hamlib_sidecar_linux.py --rigctld-port 4535 \
                                --user-backend pulseaudio-iq \
                                --iq-rate 192000
```

### `hamlib_sidecar_portable.py` (any OS)

GNU Radio ZMQ bridge or direct SoapySDR hardware. Four ZMQ endpoints
exposed (audio RX/TX + IQ RX/TX); the audio path is bidirectional, IQ
TX is honored if the backend defines it (TCI does not — TX IQ is
discarded for TCI).

```bash
python3 hamlib_sidecar_portable.py --rigctld-port 4534 \
                                   --user-backend gnuradio
# ZMQ binds: tcp://*:5550 RX audio, *:5551 TX audio,
#            *:5552 RX IQ,    *:5553 TX IQ
```

### `hamctl` (runtime CLI)

Drive frequency, mode, AGC, every DSP level, the one-shot or live
bargraph S-meter, PulseAudio loopback routing, favorites, and a
band-aware `tune <khz>` verb that consults a JSON band plan and
auto-applies mode/width/AGC. Talks to rigctld over its CAT port.

```bash
./hamctl tune 14070        # tune; auto-apply 20 m SSB profile (USB 2.4 kHz)
./hamctl smeter live       # realtime S-meter bargraph (q to quit)
./hamctl show              # full radio + DSP + audio routing snapshot
./hamctl audio attach <pulse-sink-substring>
```

### `hamlib.sh` (lifecycle script)

Wraps rigctld + sidecar + an optional PulseAudio loopback into a
single `start` / `stop` / `restart` / `status` command. Has built-in
defaults for each supported radio (`--radio tci`, `--radio kiwisdr`,
`--radio rtlsdr`). The TCI form is shown in the Quick Start above.

### TX gain

The sidecar's built-in SSB modulator and the demodulated-audio path
both honor the radio's expected drive level. ExpertSDR3's silent
threshold is mediated by the sidecar's SmartSidecar AGC and TX
modulator; see the sidecar repo's `USERS.md` and `DEVELOPERS.md` if
you need to adjust the TX gain envelope.

## Surprising Detail: TCI 2.0 IQ is RX-Only

The TCI 2.0 spec defines `IQ_STREAM` (stream_type=0) as **unidirectional** from radio to client. There is no `TX_IQ_STREAM` or any mechanism for a client to push IQ samples back to the radio for transmission. This is a protocol limitation, not a Hamlib choice.

TX of arbitrary baseband signals remains possible via the audio path (`TX_AUDIO_STREAM`), which ExpertSDR3 accepts at 8 kHz mono for HF digital modes.

Adding TX IQ outside the spec would break interop with non-Expert-Electronics TCI implementations (e.g. Apache Labs ANAN with TCI firmware).

## Lifecycle Management

The `hamlib.sh` script in `hamlib-audio-sidecar` manages rigctld + sidecar +
audio routing as a unit:

```bash
./hamlib.sh start --radio tci --host 127.0.0.1 --port 50001 \
                  --stream audio --sink <pulse-sink-substring>
./hamlib.sh stop                          # kill processes, unload sinks
./hamlib.sh restart [opts...]             # stop + start
./hamlib.sh status                        # processes, sinks, loopbacks
```

State is kept under `$XDG_RUNTIME_DIR/hamlib-sdr/`. The script handles
the rigctld invocation (model 12, `-C audio_port=4534` or `-C iq_port=4535`
depending on `--stream`), starts the sidecar, and creates the
PulseAudio loopback if `--sink` is given.

## Verified End-to-End

- **JS8Call** (14.079 MHz, 40 m): CQ got replies. On-air decode confirmed by remote receivers.
- **1 kHz tone TX** (pacat via PulseAudio sidecar): clean carrier at 14.0790 MHz, -27 to -33 dBm sustained. Zero silence frames mid-transmission.
- **RX audio** (parec from `hamlib-rx.monitor`): 8000 samples/sec, 98% nonzero, peak ~1100.
- **IQ stream** (`hamlib_sidecar_portable.py --user-backend gnuradio`): effective rate matches configured `iq_rate`.

## Testing

```bash
# CAT
echo 'f' | nc -w 1 localhost 4532    # dial freq
echo 'm' | nc -w 1 localhost 4532    # mode + width

# Audio sidecar running: RX audio flows into hamlib-rx.monitor
timeout 3 parec --device=hamlib-rx.monitor --rate=8000 --channels=1 \
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

# Portable sidecar (gnuradio backend): RX IQ reachable on ZMQ
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
# Expect effective rate ≈ iq_rate (default 192000)
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

- **Linux** — working. PulseAudio audio path + IQ-as-stereo soundcard path
  (`hamlib_sidecar_linux.py`); cross-platform GNU Radio ZMQ path and
  SoapySDR direct-hardware path (`hamlib_sidecar_portable.py`).
- **Windows** — same wire protocol, different audio APIs (planning notes
  in `windows.md`, not yet built). `hamlib_sidecar_portable.py` (ZMQ +
  SoapySDR) already runs on Windows as-is.
- **macOS** — same wire protocol, different audio APIs (planning notes
  in `osx.md`, not yet built). `hamlib_sidecar_portable.py` already runs
  on macOS as-is.

## References

In this repo:

- **TCI Protocol PDF:** `TCI_Protocol.pdf`
- **TCI backend reference:** `rigs/dummy/README-TCI-2.0.md`
- **Sidecar architecture (Hamlib side):** `README.audio-sidecar.md`
- **Sidecar API:** `docs/sidecar-api.md`

In the sidecar repo (https://github.com/jfrancis42/hamlib-audio-sidecar):

- **`PROTOCOL.md`** — canonical wire-protocol specification
- **`README.md`** — top-level overview, install, quick start
- **`USERS.md`** — operator guide (`hamlib.sh` + `hamctl`, JS8Call / fldigi /
  WSJT-X / gqrx setup, audio routing, troubleshooting)
- **`DEVELOPERS.md`** — writing a new sidecar or extending the Python code
- **`linux.md`** — Linux PulseAudio virtual-device internals

External:

- **Working C++ TCI reference:** https://github.com/maksimus1210/TCI
- **Working Python TCI reference:** eesdr-tci library (`pip install eesdr-tci`)

## License

Same as Hamlib (LGPL 2.1+).
