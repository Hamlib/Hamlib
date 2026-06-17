# TCI 2.0 Backend for Expert Electronics SunSDR Radios

Hamlib backend (RIG_MODEL_TCI2, dummy family model 12) for the
Transceiver Control Interface (TCI) protocol version 2.0.  Tested
against the SunSDR2 Pro running ExpertSDR3 v1.1.7.  The same protocol
is also implemented by other Expert Electronics SDRs (SunSDR2 DX, MB1,
ColibriNANO running ExpertSDR2/3) and by Apache Labs ANAN radios when
running TCI-capable firmware -- see "Other TCI devices" below.

TCI is a WebSocket push protocol, so this backend lives in `rigs/dummy/`.
The backend does an HTTP/1.1 upgrade handshake over Hamlib's existing
TCP layer (no external WebSocket library), drains the server's READY
dump on connect, and keeps a local state cache updated by every
incoming push.  Reads come from the cache or issue explicit queries;
writes send the appropriate TCI command.

Default path: `127.0.0.1:50001`.

## What works (CAT)

- **Frequency**: VFO A and VFO B (set/get), split TX frequency
- **Mode**: every mode in the server's MODULATIONS_LIST (AM, LSB,
  USB, CW, NFM, WFM, RTTY, DIGL, DIGU on SunSDR2 Pro)
- **PTT**: RIG_PTT_ON, RIG_PTT_ON_MIC, RIG_PTT_ON_DATA (TCI sources:
  default, Mic, tci)
- **Split** and VFO switching
- **RIT and XIT** (enable/offset)
- **Levels**: AF, RF drive, squelch, NB threshold, AGC (off/fast/
  normal), CW keyer speed, S-meter, SWR, TX power
- **Functions**: NB, NR, ANF, mute, tune (ATU)
- **CW**: send_morse (CW_MACROS with TCI escape encoding), stop_morse,
  wait_morse
- **Config params**: `trx`, `txsource`, `digl_offset`, `digu_offset`,
  `audio_port`

## Architecture: where the duties are split

There are two cooperating processes when audio is in the picture:

1. **rigctld** (this backend) -- owns the one allowed TCI WebSocket
   connection to the radio.  Speaks CAT to applications on its normal
   Hamlib port (default 4532).  Proxies audio and a small set of
   control events to and from a sidecar over a documented TCP
   sidechannel.
2. **The audio sidecar** -- a separate, OS-specific process that owns
   all platform audio plumbing.  Reads and writes the sidechannel,
   exposes virtual audio devices that JS8Call / fldigi / WSJT-X / etc.
   plug into as if they were soundcards.

The split exists because TCI carries CAT *and* audio over the same
WebSocket and the radio will only stream audio to the single TCI
client that asserted PTT.  rigctld must own that one connection; the
audio path therefore *has* to flow through rigctld.  But Hamlib is a
radio-control library, not an audio framework, and putting libpulse,
CoreAudio and WASAPI inside it would balloon its dependency surface
and split the codebase along per-OS audio boundaries.

So:

- **rigctld** stays portable C with no new audio dependencies.  It
  forwards opaque audio bytes; it does not decode, resample, gain-
  adjust, mix or otherwise interpret them.
- **The sidecar** is per-OS, written in whatever language fits, and
  fully replaceable.  It owns the platform-specific audio APIs and
  the virtual-device creation.

If `audio_port` is not configured, the backend behaves like a
conventional CAT-only Hamlib driver: no listen socket, no audio
thread, no sidechannel allocations, nothing for a reviewer to read
beyond standard backend territory.

## The TCI WebSocket (radio <-> rigctld)

Spoken at `127.0.0.1:50001` (the radio's TCI server).  Mixed text and
binary on a single WebSocket connection:

- **Text frames** -- CAT-like commands and responses
  (`VFO:0,0,14078000;`, `MODULATION:0,DIGU;`, `TRX:0,true,tci;`,
  `TX_CHRONO:0,512;`, etc.).  Handled inside the backend.
- **Binary frames** -- 64-byte TCI header followed by audio (or
  eventually IQ) payload.  rigctld forwards these to the sidecar
  unchanged when `audio_port` is enabled.

The backend's reader thread is the **sole reader** of this socket for
the lifetime of the open: it queues text frames for the CAT thread to
consume (preventing a race where audio code eats CAT replies) and
forwards binary frames to the sidecar.  `tci2_send` is internally
serialised so the reader thread (which sends AUDIO_START etc on
sidecar-connect) and the CAT thread (which sends normal Hamlib
commands) can both write to the WebSocket without colliding.

A few protocol quirks worth knowing:

- The server sends command keywords lowercase; the backend uppercases
  them on receive so sscanf format strings work.
- Mode strings come entirely from the server's `MODULATIONS_LIST`,
  so we only ever ask for modes the server supports; an unknown mode
  returns `-RIG_EINVAL`.
- `set_mode` deliberately does *not* write `RX_FILTER_BAND`:
  ExpertSDR3 auto-applies a sensible filter on `MODULATION` change,
  and writing the filter immediately after `MODULATION` corrupts the
  server's filter state.
- `VFO_LOCK` (TCI 2.0) is server-to-client only; `set_func LOCK`
  returns `-RIG_ENAVAIL`.
- `CW_MACROS_SPEED` and `CW_KEYER_SPEED` are separate; `get_level
  KEYSPD` returns the macro speed from the cache.

## The sidechannel (rigctld <-> sidecar)

Enabled by `-C audio_port=N`.  rigctld opens a TCP listen socket on
`127.0.0.1:N` (loopback only), accepts one sidecar connection, and
exchanges **length-framed binary TCI frames** with it -- in both
directions, with no text framing of any kind.

A single binary protocol covers audio *and* control events.  This
matters because audio payloads contain arbitrary bytes (including
0x0A, ':', and whitespace).  Any text-line framing on this socket
would shred frames the moment a sample value happened to be a
newline byte.  Going all-binary with the same 64-byte TCI header the
WebSocket already uses eliminates that whole class of bug.

### Frame format

Every message is a 64-byte header followed by 0..N payload bytes.

Header (16 little-endian uint32 words, total 64 bytes):

| Offset | Word | Field        | Audio frames                          | Control frames |
|--------|------|--------------|---------------------------------------|----------------|
| 0      | [0]  | receiver     | trx index                             | trx index      |
| 4      | [1]  | sample_rate  | Hz                                    | 0              |
| 8      | [2]  | format       | 0=int16  1=int24  2=int32  3=float32  | 0              |
| 12     | [3]  | codec        | 0                                     | 0              |
| 16     | [4]  | crc          | 0                                     | 0              |
| 20     | [5]  | length       | samples in payload                    | control value  |
| 24     | [6]  | stream_type  | 1=RX_AUDIO  2=TX_AUDIO                | 3=TX_CHRONO  4=PTT_STATE |
| 28     | [7]  | channels     | 1                                     | 1              |
| 32..63 |      | reserved     | zero-filled                           | zero-filled    |

Payload size = `length × channels × sample_bytes(format)` for audio
frames, **0** for control frames.

### Stream types

| stream_type | name       | direction         | length means          | payload |
|-------------|------------|-------------------|-----------------------|---------|
| 1           | RX_AUDIO   | rigctld → sidecar | samples in payload    | yes     |
| 2           | TX_AUDIO   | sidecar → rigctld | samples in payload    | yes     |
| 3           | TX_CHRONO  | rigctld → sidecar | samples requested     | no      |
| 4           | PTT_STATE  | rigctld → sidecar | 0=PTT off, 1=PTT on   | no      |

Stream types 5..255 are reserved for future hamlib-internal control
frames -- VFO change events, mode change events, sample-rate
negotiation, IQ stream start/stop, and anything else we encounter.
The 32 reserved bytes in the header give room for parameters that
don't fit in `length`.

Receivers MUST silently skip unknown stream_type values.  This is the
forward-compatibility contract: a newer rigctld emitting a
stream_type the sidecar doesn't yet handle should not break the
sidecar, and vice versa.

### What flows when

- On TCI READY, rigctld negotiates audio format with the server
  (`AUDIO_SAMPLERATE`, `AUDIO_STREAM_SAMPLE_TYPE`,
  `AUDIO_STREAM_CHANNELS`) and starts the stream (`AUDIO_START`).
  This is rigctld-initiated and invisible to the sidecar.
- Inbound RX_AUDIO_STREAM TCI binary frames from the radio are
  forwarded verbatim to the sidecar as `STREAM_RX_AUDIO` frames.
- The text command `TX_CHRONO:trx,samples;` from the radio is
  rewritten as a `STREAM_TX_CHRONO` control frame (length = samples
  requested, no payload) and sent to the sidecar.
- The sidecar replies with a `STREAM_TX_AUDIO` frame whose payload
  is exactly that many samples.  rigctld reassembles it across
  recv() boundaries (TCP is a byte stream) and forwards it to the
  radio as a TX_AUDIO_STREAM TCI binary frame.
- When an application calls `tci2_set_ptt()`, on every PTT *edge*
  (only on edges, not on every set_ptt call), rigctld emits a
  `STREAM_PTT_STATE` control frame so the sidecar knows when a TX
  cycle starts and ends.

### Why rigctld owns the PTT_STATE event

rigctld is the authoritative PTT source on the sidechannel.  An
application calls `set_ptt(1)` against rigctld; rigctld tells the
radio to key.  We do not depend on the radio echoing TRX state back
over the WebSocket (it sometimes does, sometimes doesn't, depending
on configuration).  Emitting PTT_STATE from inside `tci2_set_ptt`
makes the contract simple: PTT on the sidechannel is what rigctld
just commanded, full stop.

The sidecar uses PTT_STATE: ON to flush its TX capture buffer at the
start of every transmission.  Without this, audio captured by the
sidecar's input pipeline while the radio was idle (silence on most
platforms, but always *something*) would be queued and shipped to the
radio on the next PTT-on, delaying the live audio.  With PTT_STATE
the sidecar flushes that stale data the moment the user keys.

## ExpertSDR3 quirks worth knowing

- ExpertSDR3 silently drops TX audio frames whose payload is below
  some internal level threshold.  TX engages, TX_CHRONO/TX_AUDIO
  flow, the PA emits zero watts.  No error response.  This bites
  JS8Call particularly hard because JS8Call writes audio at peak
  ~3000 (about -20 dBFS), well below ExpertSDR3's threshold.  The
  sidecar handles this with a configurable post-amplifier on its TX
  path; the backend forwards bytes and does not modify levels.
- After many fast disconnects/reconnects, ExpertSDR3 occasionally
  wedges its audio engine; killing every ExpertSDR3 process and
  letting it restart clears it.  Operational, not a backend issue.

## Other TCI devices

The TCI 2.0 spec is published by Expert Electronics, but the protocol
itself is straightforward and documented; nothing about this backend
is ExpertSDR3-specific beyond the small quirks listed above.  Devices
that should work with this backend, in principle:

- **Expert Electronics**: SunSDR2 Pro / DX (verified), MB1,
  ColibriNANO running ExpertSDR2/3
- **Apache Labs ANAN**: ANAN-7000DLE, ANAN-8000DLE, ANAN-G2 etc.
  when running TCI-capable firmware (Thetis or compatible builds
  that expose TCI on port 50001)

Adding support for a new TCI implementation should normally require
no code change -- if the device follows the spec, it Just Works.
What may need work:

- Additions to the modulation map if the radio reports modes the
  current code doesn't recognise.  The runtime mode table is built
  from the server's `MODULATIONS_LIST` and is intentionally generic;
  rare vendor-specific mode names just need a mapping.
- Per-vendor quirk handling, similar to the ExpertSDR3 quirks above.
  These are easiest to add as small conditional branches once
  observed; please file an issue with a packet capture if you find
  one.

**Flex Radio is *not* a TCI device.**  Flex uses its own proprietary
API (the SmartSDR network API) and is not addressed by this backend.

## Reference sidecars

A reference Linux sidecar lives at
`https://github.com/jfrancis42/hamlib-tci-sidecar`.  It creates two
PulseAudio null sinks (`tci-rx`, `tci-tx`) using `pactl`, plays RX
audio into the rx sink with `pacat`, captures TX audio from the tx
sink's monitor with `parec`, and bridges both directions to rigctld
over the audio sidechannel.  Modems point at standard PulseAudio
device names and never know they're talking to a SunSDR2.  Works on
any current desktop Linux that has either PulseAudio or PipeWire with
the pipewire-pulse shim, which is essentially all of them.

The protocol code in any sidecar is a few hundred lines: parse the
64-byte header, dispatch on `stream_type`, push RX_AUDIO payloads to
the platform's playback path, capture TX audio from the platform's
recording path, ship TX_AUDIO frames in response to TX_CHRONO,
respond to PTT_STATE edges by flushing the TX capture buffer.
That's it.  Everything else is platform glue.

### Per-OS sidecars

Audio APIs differ between operating systems and there is no portable
backend that works well across all three majors without compromise.
The protocol design above puts the platform boundary cleanly between
rigctld (one C codebase, all platforms) and the sidecar (one per
platform, free to use whichever native API is best on that OS):

- **Linux**: PulseAudio / PipeWire null sinks plus `pacat` and
  `parec`.  This is the reference implementation and the most
  thoroughly tested today.
- **Windows**: planned, likely targeting **WASAPI** for the audio
  glue and **VB-Audio Cable** as the virtual device counterpart of
  null sinks.  The Python `sounddevice` library covers WASAPI
  cleanly; the protocol code is identical to the Linux sidecar.
- **macOS**: planned, likely targeting **CoreAudio** with
  **BlackHole** or a CoreAudio aggregate device for the virtual
  audio.  Again, `sounddevice` is the natural choice.

The decoupling means a Windows or macOS user gets the same rigctld
binary the Linux user has and just runs a different sidecar.  No
multi-platform `#ifdef` jungle in Hamlib.

### GNU Radio integration

A future sidecar can present the TCI audio (and IQ -- see below) as a
GNU Radio source/sink instead of as virtual soundcards.  This is a
natural fit for the architecture: the protocol code stays the same,
the sidecar exposes ZMQ or UDP ports (or a pair of native
`gr-blocks`) so a GNU Radio flowgraph can pull RX audio/IQ and push
TX audio/IQ as ordinary GR streams.  No changes to rigctld are
required for this; only the sidecar needs to know about GR.

This opens up using a SunSDR2 (or ANAN) as a GNU Radio frontend with
full CAT control, while still letting Hamlib see the same radio for
non-GR clients on the same machine.

## IQ streaming (planned)

The current version handles audio only.  TCI also defines binary IQ
streams.  Like audio, the IQ stream is delivered to whichever client
asserted PTT, so the same proxy architecture applies: the radio sends
IQ frames over the WebSocket, rigctld forwards them as a new
stream_type to the sidecar, and the sidecar delivers them to whatever
consumer wants them (a SDR application, GNU Radio, a recorder, ...).

Adding IQ on top of the current protocol is intentionally cheap:

- A new constant, e.g. `STREAM_IQ_AUDIO = 5` (and `STREAM_IQ_TX` for
  TX-side IQ if a future use case emerges).
- A new dispatch arm in the sidecar that pushes IQ payloads to its
  IQ sink (a UDP port, a named pipe, a GR block -- sidecar's
  choice).
- Negotiation: rigctld already speaks the relevant TCI text commands
  (`IQ_SAMPLERATE`, `IQ_OUTPUT_SAMPLE_TYPE`, `IQ_START` / `IQ_STOP`).
  Existing sidecars that don't know about stream_type 5 silently
  skip it; a new sidecar that does will receive the frames.

No frame-format change is required.  The 32 reserved header bytes
give room to add IQ-specific metadata (typical: nominal centre
frequency, decimation, RX/TX direction flag) without breaking
existing implementations.

## Verification

End-to-end on a SunSDR2 Pro running ExpertSDR3 v1.1.7:

- **CAT via rigctld**: frequency / mode / split / PTT cycle with
  cross-checks against ExpertSDR3's GUI.
- **RX audio**: 8000 samples/sec captured cleanly from the sidecar's
  `tci-rx.monitor` null source, full noise-floor signal at expected
  amplitude.
- **TX audio (bench tone)**: 1 kHz tone via pacat into `tci-tx`
  produces a clean carrier 1 kHz above the dial frequency in USB on
  a Siglent SSA3032X Plus, sustained throughout the transmission
  with no silence frames inside the live audio region.
- **TX audio (real JS8 frame)**: clean modulated carrier on the SSA
  for the full ~13-second frame, no dropouts.
- **PTT_STATE control frames**: confirmed firing on every PTT edge,
  triggering the sidecar's pre-TX buffer flush.
- **On-air decode**: 40 m JS8 CQ via this pipeline received and
  replied to by remote stations, confirming end-to-end on-air
  timing is within decoder tolerance.
