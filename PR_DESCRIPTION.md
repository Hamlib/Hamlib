# TCI 2.0: Add full audio and IQ support via external sidecars

This PR extends the TCI 2.0 backend (`rigs/dummy/tci2.c`) with complete audio and IQ streaming support through an external sidecar architecture.

## What This Adds

**Audio support** (bidirectional RX/TX) and **IQ support** (RX-only) for TCI 2.0, compatible with JS8Call, fldigi, WSJT-X, GNU Radio, and other ham radio software.

The implementation uses **external sidecar processes** connected via TCP to rigctld, keeping Hamlib's C codebase free of platform-specific audio dependencies while enabling full TCI functionality.

## Architecture

```
ExpertSDR3 (TCI server, port 50001)
        |
        | WebSocket (one connection)
        |
    rigctld (:4532 CAT, :4534 audio sidechannel, :4535 IQ sidechannel)
        |
        +--- audio sidecar (Python) ---> PulseAudio / ZMQ
        |                                   |
        |                                   v
        |                          JS8Call / fldigi / GNU Radio
        |
        +--- IQ sidecar (Python) -------> ZMQ (complex float32)
                                             |
                                             v
                                      GNU Radio / SDR apps
```

**rigctld** remains the sole TCI WebSocket client to ExpertSDR3. Audio and IQ streams are proxied as **length-framed binary TCI frames** over independent TCP sidechannels (ports 4534 and 4535) to external Python sidecar processes. The sidecars handle platform-specific audio plumbing (PulseAudio on Linux, planned: WASAPI/VB-Cable on Windows, BlackHole/CoreAudio on macOS) and ZMQ publish for SDR consumers.

## Why External Sidecars?

1. **No new Hamlib dependencies** — rigctld stays pure C with existing dependencies; no libpulse/coreaudio/wasapi.
2. **Platform flexibility** — Linux, Windows, and macOS sidecars can use completely different audio APIs without touching Hamlib code.
3. **Process isolation** — audio/IQ bugs don't crash rigctld (and the CAT connection with it).
4. **Fast iteration** — audio pipeline changes are Python edits + restart, not Hamlib rebuild + redeploy.

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
- `STREAM_IQ (0)` — receiver IQ (rigctld → IQ sidecar, RX-only — **TCI 2.0 does not define TX IQ**)
- `STREAM_RX_AUDIO (1)` — receiver audio (rigctld → audio sidecar)
- `STREAM_TX_AUDIO (2)` — transmit audio (audio sidecar → rigctld)
- `STREAM_TX_CHRONO (3)` — hamlib-internal control frame: radio requests N samples (rigctld → audio sidecar)
- `STREAM_PTT_STATE (4)` — hamlib-internal control frame: PTT state change (rigctld → audio sidecar)

Values 0..2 match TCI's own `StreamType` enum and are forwarded verbatim from ExpertSDR3. Values 3+ are hamlib-internal control between rigctld and the sidecars.

**Why binary-only?** Audio and IQ payloads contain arbitrary bytes (including 0x0A). Early prototypes that mixed text lines (`TX_CHRONO 0 512\n`) with binary frames on the same socket corrupted audio whenever a sample value happened to contain a newline. Going all-binary with TCI's existing header eliminates that entire class of bug.

## Key Code Changes in `tci2.c`

### 1. IQ sidecar support (new in this version)

- **`iq_port` and `iq_rate` config params** (`-C iq_port=4535 -C iq_rate=192000`).
- **`tci2_iq_init()` / `tci2_iq_cleanup()` / `tci2_iq_accept()`** — parallel structure to the audio sidechannel. Listen socket on `iq_port`, accepts one IQ sidecar at a time.
- On IQ sidecar connect: rigctld sends `IQ_SAMPLERATE:N;` and `IQ_START:trx;` to ExpertSDR3.
- **Shared poll thread** — `tci2_audio_poll_thread` (audio's reader) is the sole WebSocket reader and dispatches binary frames by `stream_type`:
  - `STREAM_IQ (0)` → `iq_sidecar_fd`
  - `STREAM_RX_AUDIO (1)` / `STREAM_TX_CHRONO (3)` → `audio_sidecar_fd`
- **WebSocket receive buffer raised to 32 KB** (`TCI2_WS_BUFLEN`) to accommodate IQ frames at 384 kHz (8 KB typical, 16 KB burst). The old 8 KB was sized for audio only.

**Note: RX IQ only.** TCI 2.0's `StreamType` enum defines exactly one IQ stream (`IQ_STREAM = 0`), unidirectional from radio to client. There is no spec-defined way to push IQ samples back to the radio for transmission. The IQ sidechannel and sidecar process are RX-only. TX of arbitrary baseband audio remains available via the audio sidecar's `TX_AUDIO_STREAM` path.

### 2. Audio sidecar support (refined from prior commits)

- **Single-reader queue** between the WebSocket reader thread and the CAT thread. The audio poll thread is the sole reader; text frames → 64-slot ring buffer; binary frames → sidecars. CAT thread pops from the queue. Fixes race where the audio thread ate CAT replies (was RPRT -5 on first command).
- **TCP-stream reassembly** of TCI frames from the audio sidecar. A partial `recv()` used to forward a malformed WebSocket frame (ExpertSDR3 silently dropped it → 0 W TX). Now accumulates into `priv->audio_buf`, parses the 64-byte header to learn total frame size, only emits when complete.
- **`tci2_send` thread-safe** — acquires `priv->ws_mutex` internally. The audio thread and CAT thread can now both write to the WebSocket safely.
- **TX-from-sidecar pumping every iteration**, not only on WebSocket poll timeout. With sensors streaming continuously, poll() almost never times out. Old code only drained sidecar bytes on timeout → chunks mangled into single frames.
- **TX_CHRONO as binary `STREAM_TX_CHRONO` control frame** (not text) — makes the binary-only sidechannel possible.
- **PTT edges as `STREAM_PTT_STATE` control frames** inside `tci2_set_ptt()`. rigctld is the authoritative PTT source on this socket; we don't depend on ExpertSDR3 echoing TRX state. The sidecar uses this to flush its TX capture buffer at the start of each transmission (avoiding shipping stale silence).
- **Idempotent audio listen-socket setup** — `tci2_audio_init` is called on every TCI READY (= every CAT reconnect). If `audio_listen_fd >= 0`, skip bind/listen and just restart the reader thread. Prevents "address already in use" on CAT churn.
- **Audio thread joins on `tci2_close`** — the old code kept it alive across CAT reconnects, spinning on `POLLNVAL` once the WebSocket FD was closed. Now torn down in `tci2_close`, restarted in `tci2_audio_init`.

### 3. Config parameters

- `trx` (int, 0-based) — which TRX to control on multi-receiver hardware (e.g. SunSDR2 DX has two).
- `txsource` (string: `default` / `mic` / `vac`) — audio source when keying TX. Overridden per-PTT-call by `RIG_PTT_ON_MIC` (mic) / `RIG_PTT_ON_DATA` (vac).
- `digl_offset`, `digu_offset` (int, 0..4000 Hz) — frequency offsets for DIGL/DIGU modes.
- **`audio_port`** (int, 0=disabled) — TCP port for the audio sidechannel. Auto-enables TCI audio (`AUDIO_START`).
- **`iq_port`** (int, 0=disabled) — TCP port for the IQ sidechannel. Auto-enables TCI IQ stream (`IQ_START`). Independent of `audio_port`; both can be enabled together.
- **`iq_rate`** (int, default 192000) — sample rate (Hz) requested for the TCI IQ stream when `iq_port` is enabled. ExpertSDR3 supports 48000 / 96000 / 192000 / 384000.

## Sidecar Implementations (not part of this PR)

The sidecar processes live in a separate repo (`hamlib-tci-sidecar`). Three implementations:

1. **`tci-audio-soundcard-sidecar.py`** — PulseAudio/PipeWire (Linux). RX/TX audio as null sinks for JS8Call, fldigi, WSJT-X, etc. Bidirectional.
2. **`tci-audio-gr-sidecar.py`** — GNU Radio audio bridge. RX audio published as ZMQ PUB (float32 mono 8 kHz); TX audio accepted on ZMQ PULL. Alternative to the PulseAudio sidecar. Bidirectional.
3. **`tci-iq-sidecar.py`** — IQ bridge. Receiver IQ stream as ZMQ PUB (complex float32). **RX-only** (TCI does not define TX IQ). For GNU Radio and other ZMQ-aware SDR consumers.

Plus two GR-side tools (`tci-iq-viewer.py` — Qt FFT + waterfall with click-to-tune; `tci-audio-gr-tester.py` — Qt FFT + 1 kHz tone generator with PTT button).

Windows and macOS audio sidecars are planned (same wire protocol, different audio APIs). The IQ sidecar and GR audio sidecar are portable as-is (ZMQ + numpy, no platform-specific audio).

## Verified End-to-End

- **JS8Call** (2026-06-07, 14.079 MHz, 40 m): CQ via this pipeline got replies. On-air decode confirmed by remote receivers. ~2 s pre-roll intrinsic to JS8Call's slot scheduler, not this pipeline (measured with and without `OutputBufferMs=200` setting — no change). Real-world latency tolerable.
- **1 kHz tone TX** (pacat via PulseAudio sidecar): clean carrier at 14.0790 MHz, -27 to -33 dBm sustained, RFPOWER_METER ~50 mW. Zero silence frames mid-transmission.
- **RX audio** (parec from `tci-rx.monitor`): 8000 samples/sec, RMS ~320, peaks ~1100, one frame with peak 13587 (real signal). 98% nonzero.
- **IQ stream** (via `tci-iq-sidecar.py` + `tci-iq-viewer.py`): ~280 ZMQ messages, ~580k complex samples in 3 s. Effective rate matches configured `iq_rate` (192000 Hz default). Live FFT + waterfall tracks dial frequency; click-to-tune works via rigctld CAT.

## Surprising Detail: TCI 2.0 IQ is RX-Only

The TCI 2.0 spec defines `IQ_STREAM` (stream_type=0) as **unidirectional** from radio to client. There is no `TX_IQ_STREAM` or any mechanism for a client to push IQ samples back to the radio for transmission. This is a protocol limitation, not a Hamlib choice. TX of arbitrary baseband signals remains possible via the audio path (`TX_AUDIO_STREAM`), which ExpertSDR3 accepts at 8 kHz mono for HF digital modes.

Adding TX IQ outside the spec would break interop with non-Expert-Electronics TCI implementations (e.g. Apache Labs ANAN with TCI firmware) and isn't actionable without cooperation from the TCI spec authors.

## Build Notes

The Hamlib build system silently uses stale convenience archives when only `tci2.c` is touched. A clean rebuild requires:

```bash
make -C rigs/dummy tci2.lo
rm -f rigs/dummy/.libs/libhamlib-dummy.a rigs/dummy/libhamlib-dummy.la
( cd rigs/dummy && make libhamlib-dummy.la )
rm -f src/libhamlib.la src/.libs/libhamlib.so.5*
( cd src && make libhamlib.la )
touch tests/rigctld.c && make -C tests rigctld
```

Verify the new code is in the .so:

```bash
strings src/.libs/libhamlib.so.5.0.0 | grep -E "PTT_STATE forward|IQ sidecar listening"
# Both strings must appear
```

## Testing

Basic sanity:

```bash
# CAT
echo 'f' | nc -w 1 localhost 4532    # dial freq
echo 'm' | nc -w 1 localhost 4532    # mode + width

# Audio sidecar running: RX audio flows into tci-rx.monitor
timeout 3 parec --device=tci-rx.monitor --rate=8000 --channels=1 \
    --format=s16le > /tmp/rx_check.raw
# Expect 24000 samples, peak well above 100, fraction nonzero > 95%

# IQ sidecar running: IQ stream reachable on ZMQ
python3 -c "
import zmq, time
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
```

Ultimate test: **call CQ with JS8Call on a band where someone is listening**. If you get replies, the entire audio pipeline is working on-air.

## Related Documentation

- **TCI Protocol PDF:** `TCI_Protocol.pdf` in this repo
- **Sidecar repo:** https://github.com/jfrancis42/hamlib-tci-sidecar (includes `PROTOCOL.md` — wire-protocol spec; `README.md` — sidecar usage; `tci.sh` — lifecycle manager)
- **Working C++ TCI reference:** https://github.com/maksimus1210/TCI
- **Working Python TCI reference:** eesdr-tci library (pip install eesdr-tci)

## License

Same as Hamlib (LGPL 2.1+).

## Acknowledgements

- Expert Electronics for the TCI 2.0 protocol specification.
- eesdr-tci and the C++ TCI reference for showing how a working TCI client behaves.
- The Hamlib project for the foundation this builds on.
