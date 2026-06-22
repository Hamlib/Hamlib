# TCI 2.0: Add full audio and IQ support via external sidecars

This PR extends the TCI 2.0 backend (`rigs/dummy/tci2.c`) with complete
audio and IQ streaming support through an external sidecar
architecture, plus a small reusable sidecar library
(`include/hamlib/sidecar.h`, `src/sidecar.c`) that's intended to drive
the same protocol for other backends later.

> ## 📄 Read the protocol spec first
>
> The **complete, canonical, language-agnostic sidecar wire-protocol
> specification** lives in the sidecar repo:
>
> **https://github.com/jfrancis42/hamlib-audio-sidecar/blob/main/PROTOCOL.md**
>
> Anyone implementing a sidecar in any language — or evaluating the
> protocol design that's the heart of this PR — should start there.
> This PR description summarizes the protocol enough to understand the
> Hamlib-side code changes, but `PROTOCOL.md` is the authoritative
> reference (frame format, every stream type, IEEE-754 encoding of
> float-valued control frames, reserved ranges, forward-compat rules,
> reference decoding snippets in C / Python / Rust / Go).

## Status of this PR — please read

This PR is being **published for an overall architecture review**, not
as a final implementation:

- It largely works. JS8Call, fldigi, and WSJT-X all run against
  ExpertSDR3 through this pipeline; on-air RX and TX are both
  functional.
- **There are still minor latent audio issues being worked out.** The
  control flow, framing, and CAT integration are stable; the
  remaining problems are around the edges of the audio path. None of
  them are blockers for the architecture discussion.
- The sidecar **protocol and library** (the `sidecar_*` API in this PR)
  are what we'd most like feedback on. Once the protocol settles,
  expect a follow-up that lands the additional backends below.

**Held back from this PR for now:** complete, end-to-end working
backends for **KiwiSDR (model 102, WebSocket)** and **RTL-SDR (model
202, librtlsdr)** have both been written and verified with live
signals (HF AM/SSB/CW, WFM stereo broadcast, airband AM, NBFM weather,
KiwiSDR IQ mode, etc.). They use exactly the sidecar API in this PR
and are exercised heavily by the same Python sidecar codebase. They're
deliberately kept out of this PR until the sidecar design is finalized
here, so we're not asking reviewers to evaluate three backends in
parallel.

## What This Adds

**Audio support** (bidirectional RX/TX) and **IQ support** (RX-only)
for TCI 2.0, compatible with JS8Call, fldigi, WSJT-X, GNU Radio, and
other ham radio software.

The implementation uses **external sidecar processes** connected via
TCP to rigctld, keeping Hamlib's C codebase free of platform-specific
audio dependencies while enabling full TCI functionality.

## Architecture

```
ExpertSDR3 (TCI server, port 50001)
        |
        | WebSocket (one connection)
        |
    rigctld (:4532 CAT, :4534 audio sidechannel, :4535 IQ sidechannel)
        |
        +--- sidecar (Python, separate repo)
                |
                +---> PulseAudio virtual sinks
                |       hamlib-rx (RX), hamlib-tx (TX)
                |       -> JS8Call / fldigi / WSJT-X / etc.
                |
                +---> GNU Radio ZMQ endpoints (any OS)
                |       audio RX/TX, IQ RX/TX (four sockets)
                |
                +---> SoapySDR direct hardware (bypasses rigctld;
                        for radios without a Hamlib backend yet)
```

**rigctld** remains the sole TCI WebSocket client to ExpertSDR3. Audio
and IQ streams are proxied as **length-framed binary TCI frames** over
independent TCP sidechannels (ports 4534 and 4535) to an external
Python sidecar process. The sidecar handles platform-specific audio
plumbing and the various downstream transports.

## Why External Sidecars?

1. **No new Hamlib dependencies** — rigctld stays pure C with existing
   dependencies; no libpulse / coreaudio / wasapi.
2. **Platform flexibility** — Linux, Windows, and macOS sidecars can
   use completely different audio APIs without touching Hamlib code.
3. **Process isolation** — audio/IQ bugs don't crash rigctld (and the
   CAT connection with it).
4. **Fast iteration** — audio pipeline changes are Python edits +
   restart, not Hamlib rebuild + redeploy.
5. **Generic protocol** — the same wire protocol works for TCI,
   KiwiSDR, RTL-SDR, and (we expect) anything else with a usable
   IQ-or-audio sidechannel. Backends only need to learn the sidecar
   library's `sidecar_send_*` and `sidecar_emit_*` calls.

## Wire Protocol (rigctld ↔ sidecar)

**Canonical spec:** the complete, language-agnostic protocol
specification is in the sidecar repo at
**https://github.com/jfrancis42/hamlib-audio-sidecar/blob/main/PROTOCOL.md**.
The summary below is just enough context for the code review here;
anything that disagrees with `PROTOCOL.md` is a bug in this summary,
not in `PROTOCOL.md`.

Pure binary, length-framed TCI frames in both directions:

```
64-byte header (16 little-endian uint32 words) + 0..N payload bytes
```

| Offset | Field       | Audio/IQ frames                    | Control frames |
|--------|-------------|------------------------------------|----------------|
| 0      | receiver    | trx index                          | trx index      |
| 4      | sample_rate | Hz                                 | 0              |
| 8      | format      | 0=int16 1=int24 2=int32 3=float32  | 0              |
| 20     | length      | samples in payload                 | control value  |
| 24     | stream_type | 0=IQ 1=RX_AUDIO 2=TX_AUDIO         | 3=TX_CHRONO 4=PTT_STATE ... |
| 28     | channels    | 1 (audio) or 2 (IQ)                | 1              |
| 32..63 | reserved    | zero-filled                        | zero-filled    |

Stream types 0–5 are data streams (IQ, RX_AUDIO, TX_AUDIO, TX_CHRONO,
PTT_STATE, TX_IQ). Types 6–19 are control frames (MODE, FREQ, SPLIT,
FILTER, AGC_LEVEL, NR_LEVEL, NB_LEVEL, NOTCH, RF_GAIN, SQUELCH,
PREAMP, ATT, CW_PITCH, APF). Types 20–24 are reserved for future
Hamlib use; 100–999 for future Hamlib growth; 1000–1999 for
vendor-specific extensions. Sidecars MUST silently skip unknown
types — that's how the protocol stays forward-compatible without
version negotiation.

**Float-valued control frames** (NR_LEVEL, NB_LEVEL, RF_GAIN,
SQUELCH, APF) put the IEEE-754 single-precision bit pattern in the
`length` field. See `sidecar.c` and the sidecar repo's
[`PROTOCOL.md`](https://github.com/jfrancis42/hamlib-audio-sidecar/blob/main/PROTOCOL.md)
for the encoding details and decoding snippets in C / Python / Rust /
Go.

**Why binary-only?** Audio and IQ payloads contain arbitrary bytes
(including 0x0A). Early prototypes that mixed text lines
(`TX_CHRONO 0 512\n`) with binary frames on the same socket
corrupted audio whenever a sample value happened to contain a
newline. Going all-binary with TCI's existing header eliminates that
entire class of bug.

## Key Code Changes in `tci2.c`

### IQ sidechannel

`iq_port` and `iq_rate` config params; `tci2_iq_init / cleanup /
accept` follow the same shape as the audio path. The audio poll thread
is the sole WebSocket reader and dispatches binary frames by
`stream_type` — IQ frames go to the IQ sidecar fd, audio frames + TX
chrono to the audio sidecar fd. The WebSocket receive buffer was
raised to 32 KB to accommodate IQ frames at 384 kHz.

**RX IQ only:** the TCI 2.0 spec defines `IQ_STREAM = 0` as
unidirectional. There is no spec-defined way to push IQ samples back
to the radio for transmission, so the IQ sidechannel and sidecar are
RX-only. TX of baseband audio remains available via the audio path
(`STREAM_TX_AUDIO`).

### Audio sidechannel

- **Single-reader queue** between the WebSocket reader thread and the
  CAT thread. Text frames → 64-slot ring buffer; binary frames →
  sidecar fds. CAT thread pops from the queue.
- **TCP-stream reassembly** of sidecar TX frames. Partial `recv()`s
  used to forward malformed WebSocket frames (ExpertSDR3 silently
  dropped them → 0 W TX). The reader now accumulates a 64-byte header
  before emitting.
- **`tci2_send` is thread-safe**; audio and CAT threads share the
  socket under `priv->ws_mutex`.
- **TX-from-sidecar pumped every iteration**, not only on WebSocket
  poll timeout — the old code mangled chunks when sensors streamed
  continuously and poll() never timed out.
- **TX_CHRONO as binary `STREAM_TX_CHRONO`** (not text) — keeps the
  sidechannel binary-only.
- **PTT edges as `STREAM_PTT_STATE`** inside `tci2_set_ptt()`. rigctld
  is the authoritative PTT source; the sidecar uses these edges to
  flush stale TX capture buffer at the start of each transmission.
- **Idempotent audio listen-socket setup** across TCI READY /
  reconnect cycles.

### Config parameters

| Token | Type | Default | Purpose |
|-------|------|---------|---------|
| `trx` | int | 0 | TRX index (0-based) for multi-RX rigs |
| `txsource` | string | `default` | `default` / `mic` / `vac`; overridden by PTT type |
| `digl_offset`, `digu_offset` | int | 0 | DIGL/DIGU freq offset, Hz (0–4000) |
| `audio_port` | int | 0 | Audio sidechannel TCP port (0 = disabled) |
| `iq_port` | int | 0 | IQ sidechannel TCP port (0 = disabled) |
| `iq_rate` | int | 192000 | TCI IQ stream rate (48k / 96k / 192k / 384k) |

`audio_port` and `iq_port` are independent; either or both can be
enabled.

## Sidecar Library (`include/hamlib/sidecar.h` + `src/sidecar.c`)

The sidecar wire format and the partial-send retry logic were
duplicated across early prototypes. This PR consolidates them into a
small library that any Hamlib backend can use:

- `sidecar_init_port(port)` — listen on `localhost:port`, return fd
- `sidecar_send_rx_audio(fd, ...)`, `sidecar_send_rx_iq(fd, ...)`
- `sidecar_send_tx_chrono(fd, ...)`, `sidecar_send_ptt_state(fd, ...)`
- `sidecar_emit_mode(fd, ...)`, `sidecar_emit_freq(fd, ...)`,
  `sidecar_emit_agc_level(fd, ...)`, etc. — one per control frame type
- `sidecar_close_port(fd)`

The library handles framing, retries `EAGAIN` on partial sends, and
encodes float values into the `length` field correctly. `tci2.c` is
the first user; the upcoming KiwiSDR and RTL-SDR backends use it
identically.

See `docs/sidecar-api.md` for the C API reference and the integration
checklist, and the sidecar repo's `PROTOCOL.md` for the canonical
language-agnostic spec.

## Sidecar Implementations (not part of this PR)

The sidecar processes live in a separate repo
(https://github.com/jfrancis42/hamlib-audio-sidecar). The Python
codebase ships two sidecars plus a runtime control CLI and a
lifecycle script:

1. **`hamlib_sidecar_linux.py`** — Linux. Selectable user-side
   backend: `pulseaudio` (virtual sinks `<prefix>-rx`, `<prefix>-tx`
   for demodulated audio in/out) or `pulseaudio-iq` (raw IQ as a
   stereo soundcard, L=I, R=Q). Bidirectional.
2. **`hamlib_sidecar_portable.py`** — Linux / macOS / Windows. GNU
   Radio ZMQ bridge (four endpoints: audio RX/TX + IQ RX/TX), or
   SoapySDR direct hardware bypassing rigctld entirely.
3. **`hamctl`** — runtime CLI: frequency, mode, AGC, every DSP level,
   one-shot or live-bargraph S-meter, audio routing (PulseAudio
   loopback management), favorites, band-aware `tune <khz>` verb that
   auto-applies mode/width/AGC from a JSON band plan.
4. **`hamlib.sh`** — lifecycle script: `start` / `stop` / `restart` /
   `status` for the Linux sidecar, with `--radio {rtlsdr|kiwisdr|tci}`
   and per-radio defaults. Brings up rigctld + sidecar + PulseAudio
   loopback together.

Both sidecars sit on top of one shared library
(`hamlib_sidecar_common.py`) which contains a built-in
software-defined-radio receiver and transmitter (SSB / AM / FM
mono+stereo / CW) plus the DSP suite. For IQ-only backends that
library is also the canonical Python reference for parsing the wire
protocol.

## Things That Will Bite You

A few non-obvious lessons accumulated during the implementation;
useful to know in advance if you're reviewing or extending this
code:

- **Binary on the sidechannel is non-negotiable.** Any text framing
  (newline-terminated records, `:` separators, etc.) will eventually
  hit an audio sample that contains the framing byte. Don't mix.
- **Send loops, not single sends.** Always loop `send()` until the
  full frame is on the wire. `sidecar_send_frame()` in `src/sidecar.c`
  is the reference; partial sends used to drop bytes mid-frame and
  produce an endless "bogus header" resync on the sidecar.
- **The `channels` field is load-bearing.** Stereo IQ payload size is
  `length × 2 × sample_bytes`, not `length × sample_bytes`. Sidecars
  that ignore `channels` will read half the bytes and either run at
  half rate or scribble over the next frame.
- **Threading discipline.** With both audio and CAT writing to the
  WebSocket, one mutex around the send path is mandatory. Without it
  you get rare interleaved-byte corruption that ExpertSDR3
  silently drops.
- **Single WebSocket reader.** ExpertSDR3 (and TCI in general) does
  not tolerate two clients on the same channel. rigctld must own the
  one WebSocket; sidecars get TCP from rigctld.
- **Skip unknown stream types.** Forward-compat depends on it. The
  sidecar library and every reference implementation already do this.
- **Frame-boundary state in the sidecar.** Decimators, resamplers,
  AGC envelopes, deemphasis — all stateful filters must thread their
  state across frame boundaries. Stateless per-frame DSP produces a
  transient at the frame-rate cadence (15–150 Hz) that sounds like
  pumping or thumping. Bit us several times during sidecar
  development.

## Verified End-to-End

- **JS8Call** (14.079 MHz, 40 m): CQ via this pipeline gets replies.
  On-air decode confirmed by remote receivers. There's a ~2 s
  pre-roll that turned out to be intrinsic to JS8Call's slot
  scheduler, not this pipeline.
- **1 kHz tone TX** (`pacat` via PulseAudio sidecar): clean carrier
  at 14.0790 MHz, -27 to -33 dBm sustained, RFPOWER_METER ~50 mW.
  Zero silence frames mid-transmission.
- **RX audio** (`parec` from `hamlib-rx.monitor`): 8000 samples/sec,
  ~98% nonzero, peaks tracking real signals.
- **IQ stream** (portable sidecar, gnuradio user-side backend):
  effective rate matches configured `iq_rate`.

## Build Notes

The Hamlib build system silently uses stale convenience archives when
only `tci2.c` is touched. If you don't see your changes take effect,
rebuild `libhamlib-dummy.la` and `libhamlib.la` explicitly. There's a
worked example in `rigs/dummy/README-TCI-2.0.md` if you hit this.

## Testing

```bash
# CAT
echo 'f' | nc -w 1 localhost 4532    # dial freq
echo 'm' | nc -w 1 localhost 4532    # mode + width

# Audio sidecar running: RX audio flows into hamlib-rx.monitor
timeout 3 parec --device=hamlib-rx.monitor --rate=8000 --channels=1 \
    --format=s16le > /tmp/rx_check.raw
# Expect 24000 samples, peak well above 100, fraction nonzero > 95%

# Portable sidecar (gnuradio user-side backend): RX IQ on ZMQ
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

Ultimate test: **call CQ with JS8Call on a band where someone is
listening**. If you get replies, the entire audio pipeline is working
on-air.

## Related Documentation

In this repo:

- **TCI Protocol PDF:** `TCI_Protocol.pdf`
- **TCI backend reference:** `rigs/dummy/README-TCI-2.0.md`
- **Sidecar architecture (Hamlib side):** `README.audio-sidecar.md`,
  `docs/sidecar-api.md`, `docs/EXTENSIBILITY-IMPLEMENTED.md`

In the sidecar repo
(https://github.com/jfrancis42/hamlib-audio-sidecar):

- **[`PROTOCOL.md`](https://github.com/jfrancis42/hamlib-audio-sidecar/blob/main/PROTOCOL.md)**
  — **canonical wire-protocol specification (language-agnostic).
  Anyone implementing a sidecar in any language should start here.**
- **[`README.md`](https://github.com/jfrancis42/hamlib-audio-sidecar/blob/main/README.md)**
  — top-level overview, install, 30-second quick start.
- **[`USERS.md`](https://github.com/jfrancis42/hamlib-audio-sidecar/blob/main/USERS.md)**
  — operator guide: JS8Call / WSJT-X / fldigi / gqrx setup, the
  `hamlib.sh` + `hamctl` workflow, audio routing, troubleshooting.
- **[`DEVELOPERS.md`](https://github.com/jfrancis42/hamlib-audio-sidecar/blob/main/DEVELOPERS.md)**
  — writing a new sidecar in any language, or extending the Python
  codebase.
- **[`linux.md`](https://github.com/jfrancis42/hamlib-audio-sidecar/blob/main/linux.md)**
  — internals of the Linux PulseAudio virtual-device backend.

External references:

- **Working C++ TCI reference:** https://github.com/maksimus1210/TCI
- **Working Python TCI reference:** eesdr-tci library
  (`pip install eesdr-tci`)

## License

Same as Hamlib (LGPL 2.1+).

## Acknowledgements

- Expert Electronics for the TCI 2.0 protocol specification.
- eesdr-tci and the C++ TCI reference for showing how a working TCI
  client behaves.
- The Hamlib project for the foundation this builds on.
