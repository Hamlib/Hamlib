# Hamlib Data Streaming Subsystem — Protocol and API Design

Design reference for the Hamlib data streaming subsystem: the C API, the
backend interface, and the rigctld UDP wire protocol. This document is the single
authoritative overview; the step-by-step backend how-to lives in
`HAMLIB_STREAMING_BACKEND_GUIDE.md`.

**Status:** experimental. The API is implemented and tested, but the wire
protocol may still change; see §14 for the current limitations.

Note that the source code is always authoritative over this document.

Key files:

* `include/hamlib/rig.h` (public types and API)
* `src/stream.{c,h}` (ring buffer + frontend)
* `src/stream_proto.{c,h}` (wire format)
* `src/stream_convert.{c,h}` (format conversion)
* `src/stream_net.{c,h}` (client-side UDP session)
* `tests/rigctld_stream.{c,h}` (rigctld feeders)

---

## 1. Scope

This document is the design reference for the Hamlib data streaming subsystem,
which adds support for **bidirectional audio and I/Q sample data streaming**
by implementing new Hamlib C API functions and a new UDP wire protocol.

The subsystem implements the following functionality:

- Backends produce RX audio and I/Q data and consume TX audio and I/Q
  data. This is the core capability that lets applications decode or
  record signals received over the air and transmit generated ones, for
  uses such as digital modes and remote operation.
- Applications reach streams through the C API by direct linking, or
  remotely over UDP through rigctld. The same application code works
  whether the radio is local or across a network.
- Formats are negotiated against each backend's capabilities, covering
  format, sample rate, and channels. An application discovers what a radio
  can do rather than assuming it, and opens only supported configurations.
- Audio is converted between PCM formats (for example PCM_S16 and
  PCM_F32), channel counts (mono and stereo), and sample rates, while
  I/Q is converted between complex formats (for example IQ_CS16 and
  IQ_CF32). An application can work in its preferred sample format
  regardless of the radio's native one.
- Absolute capture time is preserved through UTC time anchors, and a
  staleness watchdog keeps the reported accuracy honest. This enables
  recording with timestamps, correlation across receivers, and time of
  arrival work, and it prevents a stale timestamp from being trusted.
- Losses are reported to the exact sample, with statistics broken down by
  cause (gap, overrun, and link loss). An application sees exact
  discontinuities, can choose how to fill or resynchronize, and can
  distinguish a radio or network gap from a slow local consumer.
- Transmit bursts can be scheduled to a UTC instant, optionally keying
  PTT at the start and end of a burst. This enables modes that must align
  to a time window, such as FT8, WSPR, beacons, and EME, without host
  timing for every sample.
- Metadata for frequency, VFO, and PTT is aligned to sample position. A
  recording or decoder knows the exact frequency and state under which
  each sample was captured.
- Streams can be paused or resumed and muted or unmuted, and they expose
  health counters as a coherent snapshot. This supports runtime control
  and monitoring of stream quality.
- The UDP transport supports unicast, with a subscribe handshake and
  keepalive, as well as multicast RX, and it validates a token, source IP,
  and ownership for each stream. One RX stream can fan out to several
  listeners, while a deployment on a trusted LAN stays protected from
  stray or hijacked traffic.

### 1.1 Outside the scope of this subsystem

Hamlib already supports unidirectional UDP multicast of rig-state and
spectrum snapshots (JSON). This is why spectrum data streaming is out of scope,
since it works through the multicast publisher (snapshot data JSON packets via UDP).

The Hamlib data streaming protocol is not trying to solve issues that higher-level
standards, such as WebRTC, are designed to solve.  This assumption is what keeps
the protocol simple. By design, the UDP network protocol targets a trusted local
network. Operation across the public internet is expected
to run inside a VPN or similar tunnel provided by the application.

The Hamlib data streaming protocol is specifically *not* targeting:
adapting to lossy networks and varying bandwidth conditions (public networks/Internet),
connection discovery (e.g. NAT traversal, UDP hole punching), encryption,
authentication, or authorization. These are responsibilities of applications
built on top of Hamlib.

---

## 2. Architecture

Three layers, with the ring buffer as the boundary between backend and
frontend:

```
┌─────────────────────────┐    ┌─────────────────────────────────┐
│  APPLICATION (local)    │    │  APPLICATION (remote)           │
│  links libhamlib        │    │  netrigctl backend +            │
│                         │    │  src/stream_net UDP client  │
└────────────┬────────────┘    └────────────────┬────────────────┘
             │                                  ┆ UDP: 32-byte header,
             │                                  ┆ subscribe, keepalive
             │                 ┌────────────────┴────────────────┐
             │                 │  rigctld TRANSPORT (server)     │
             │                 │  feeder threads, metadata poll  │
             │                 └────────────────┬────────────────┘
             │  rig_stream_*()                  │  rig_stream_*()
             └────────────────┬─────────────────┘
                              │
┌──────────────────────────────────────────────────────────────┐
│  FRONTEND (src/stream.c)                                     │
│  stream handles · ring buffers · caps validation · stats     │
└───────────────────────────────┬──────────────────────────────┘
                                │  ring buffer (shared, in-process)
┌───────────────────────────────┴──────────────────────────────┐
│  BACKEND (rigs/<x>/..._stream.c)                             │
│  device I/O thread(s) · radio protocol · format conversion   │
└──────────────────────────────────────────────────────────────┘
```

```
RX:  hardware → backend thread → stream_ringbuf_write() → [ring buffer] → rig_stream_read()  → app
TX:  app → rig_stream_write() → [ring buffer] → stream_ringbuf_read()  → backend thread → hardware
```

Both access paths use the same `rig_stream_*()` C API: a directly-linked
application calls it in-process, while rigctld re-exports a backend's
streams over UDP and a remote application reaches them through the
netrigctl backend. rigctld is itself a frontend-API client — its feeder
threads move samples between the ring buffer and the network with the same
`rig_stream_read()`/`rig_stream_write()` calls an application uses.

The frontend allocates the stream handle and ring buffer, validates the
request against backend capabilities, then calls the backend's
`stream_open`. The backend runs its own device I/O thread(s) against the
pre-created ring buffer. The frontend never touches the radio; the
backend never touches the public handle internals beyond the documented
fields.

---

## 3. Stream model

### 3.1 Stream types

`rig_stream_type_t`:

| Type                  | Value | Direction | Channels convention |
|-----------------------|:-----:|-----------|---------------------|
| `RIG_STREAM_TYPE_AUDIO_RX` | 0     | rig → app | 1 (mono) or 2 (stereo) |
| `RIG_STREAM_TYPE_AUDIO_TX` | 1     | app → rig | 1 or 2              |
| `RIG_STREAM_TYPE_IQ_RX`    | 2     | rig → app | 1 (I/Q pair = 1 sample) |
| `RIG_STREAM_TYPE_IQ_TX`    | 3     | app → rig | 1                   |

`RIG_STREAM_TYPE_COUNT` (4) is the sentinel. Each type has an independent
lifecycle and its own slot table.

### 3.2 Sample formats

Format flags are an OR-able bitmask (`rig_stream_format_t`, a `uint32_t`).
Audio use bits 0–4, I/Q bits 16–19. Sample payload is little-endian on
the wire:

| Flag                          | Bit  | Bytes/sample | Status |
|-------------------------------|:----:|:------------:|--------|
| `RIG_STREAM_FORMAT_PCM_S8`     | 0   | 1 | implemented |
| `RIG_STREAM_FORMAT_PCM_U8`     | 1   | 1 | implemented |
| `RIG_STREAM_FORMAT_PCM_S16`    | 2   | 2 | implemented |
| `RIG_STREAM_FORMAT_PCM_F32`    | 3   | 4 | implemented |
| `RIG_STREAM_FORMAT_OPUS`       | 4   | variable | implemented as codec-frame **passthrough** (no transcode; see below) |
| `RIG_STREAM_FORMAT_IQ_CS8`     | 16  | 2 | implemented |
| `RIG_STREAM_FORMAT_IQ_CU8`     | 17  | 2 | implemented |
| `RIG_STREAM_FORMAT_IQ_CS16`    | 18  | 4 | implemented |
| `RIG_STREAM_FORMAT_IQ_CF32`    | 19  | 8 | implemented |

For I/Q, one "sample" is one complex pair: `IQ_CS16` is 4 bytes (2 each
for I and Q), `IQ_CF32` is 8 bytes. `rig_stream_format_sample_size()`
returns these; it returns 0 for compressed formats (Opus). Audio bits
5–15 are free for future formats.

**Channel interleave.** Multi-channel streams are **channel-interleaved
per sample instant**: one frame is all channels of one instant, then the
next instant. For I/Q with N channels a frame is
`I₀Q₀ I₁Q₁ … I₍N₋₁₎Q₍N₋₁₎` (each channel's complex pair in order); audio
is `L R` for stereo. `frame_bytes = sample_size × channels`, and every UDP
payload is truncated to a whole number of frames, so a frame never spans
packets. The `channels` header field is a `uint8` (1–255); the dummy
backend exercises up to 4 coherent I/Q channels. This carries **coherent**
multi-channel (shared LO/clock, e.g. X/Y polarization for EME) as one
stream; independent spectral windows are separate streams (see §7).

**Scaling and full-scale conventions.** Float samples are nominal
full-scale **±1.0**. Integer full-scale and the offset for unsigned types
(all as implemented in `src/stream_convert.c`, and the same per component
for the complex I/Q variants — I and Q each independently):

| Format | Zero | Full-scale | ↔ float32 |
|--------|:----:|:----------:|-----------|
| `PCM_S16` / `IQ_CS16` | 0 | ±32768 | f = s / 32768; s = clip(round(f × 32768), [−32768, 32767]) |
| `PCM_S8`  / `IQ_CS8`  | 0 | ±128   | f = s / 128;   s = clip(round(f × 128), [−128, 127]) |
| `PCM_U8`  / `IQ_CU8`  | 128 (offset binary) | ±128 | f = (u − 128) / 128; u = clip(round(f × 128) + 128, [0, 255]) |

Decode and encode use the **same power-of-two scale** (`32768`, `128`)
with round-to-nearest and clipping on encode: the most-negative code maps
exactly to −1.0, +1.0 clips to the positive maximum, and — the reason for
the symmetric convention — every integer sample survives an
integer → float → integer round-trip **value-exact**, which the frontend
conversion path (§3.3) depends on. 8↔16-bit integer
conversions shift by 8 bits and re-bias the offset. These conventions are
fixed wire semantics: a third-party implementation of the protocol MUST
use them so sample amplitude is interpreted identically on both ends.

The app-facing audio formats are raw PCM and Opus. Device-native
companded or compressed codecs (the μ-law and ADPCM carried on Icom
device links, for instance) are
*not* stream formats: a backend decodes them to PCM internally and
presents PCM (or Opus) upward, so applications never handle them. The
codec layer that performs that decode/encode lives in
`src/stream_codec.{c,h}`, keyed on its own `rig_audio_codec_t` enum
(never advertised in `stream_caps`); see §9.1.

`OPUS` streams are **codec-frame passthrough**: Hamlib forwards the encoded
frames untouched — radio to application (or the reverse) through the rings,
rigctld and UDP — without any codec library. A codec format is native-only
(§3.3): no conversion stage ever applies, so serving a PCM client from an
Opus-only radio (or vice versa) still requires a future **codec-layer**
transcode feature — stateful, frame-based, libopus-gated — which is *not* a
job for the stateless sample-format converter (`rig_stream_convert`). The
unit of transport is one **codec frame** (one self-contained encoded unit,
e.g. one Opus packet — distinct from the raw-PCM "frame" meaning one sample
instant): see §4 for the API semantics and §6.2.1 for the wire contract.

### 3.3 Capabilities (`struct rig_stream_caps`)

A backend declares a 0-terminated `stream_caps[HAMLIB_MAX_STREAM_CAPS]`
array in its `rig_caps`. One entry per stream type it supports:

```c
struct rig_stream_caps {
    rig_stream_type_t   type;
    rig_stream_format_t formats;                  /* OR-ed openable formats
                                                   * (a uint32_t typedef) */
    int32_t             sample_rates[HAMLIB_MAX_STREAM_RATES]; /* 0-terminated */
    int32_t             channels[HAMLIB_MAX_STREAM_CHANNEL_COUNTS];
                                                  /* openable channel counts,
                                                   * 0-terminated ascending
                                                   * list (1=mono, 2=stereo) */
    int32_t             max_streams;              /* concurrent of this type */
    int32_t             tx_schedule_horizon_ms;   /* Max timed-TX lead time
                                                   * (0 = not schedulable) */
    uint64_t            caps_flags;               /* RIG_STREAM_CAP_*, one
                                                   * 64-bit namespace, at an
                                                   * 8-aligned offset */

    /* Hardware-native view (see below) */
    rig_stream_format_t native_formats;
    int32_t             native_sample_rates[HAMLIB_MAX_STREAM_RATES];
    int32_t             native_channels[HAMLIB_MAX_STREAM_CHANNEL_COUNTS];

    uint32_t            _reserved32[1];           /* ABI headroom (tail-only;
                                                   * see the layout note) */
    uint64_t            _reserved[4];
};

/* caps_flags bits (a single 64-bit namespace; the wire carries names,
 * so future flags extend the flags= list without a new key) */
#define RIG_STREAM_CAP_TIMED_TX_COARSE  (1ULL<<0)  /* start-at-T play-out gating */
#define RIG_STREAM_CAP_TIMED_TX_SAMPLE  (1ULL<<1)  /* sample-accurate hw scheduling */
#define RIG_STREAM_CAP_BURST_PTT        (1<<2)  /* SOB/EOB auto-keys PTT */
#define RIG_STREAM_CAP_HW_TIME          (1<<3)  /* get_hardware_time meaningful */
```

Limits: `HAMLIB_MAX_STREAM_RATES = 32`,
`HAMLIB_MAX_STREAM_CHANNEL_COUNTS = 16`, `HAMLIB_MAX_STREAM_CAPS = 8`,
`HAMLIB_MAX_STREAMS = 32` (concurrent streams per type).

**Two views share the struct** — hardware-native and effective:

- A **backend** authors only the classic fields (`formats`,
  `sample_rates`, `channels`) with the **hardware-native** truth —
  what the radio actually produces or accepts — and leaves the `native_*`
  fields zero.
- An **application** reads the struct through `rig_stream_caps_at()`,
  which serves a frontend-derived copy: the classic fields are widened to
  the **effective** set (everything `rig_stream_open()` will serve,
  frontend conversion included) and `native_*` carries what the backend
  declared.

**Derivation rules** (native → effective, computed at runtime):

- **Formats:** the whole family becomes reachable — any PCM format if the
  backend has any PCM format, any I/Q format if it has any I/Q format.
  Codec format bits (e.g. Opus) pass through only as declared; they are
  never fabricated. Codec formats are opaque packet streams, so no
  conversion stage applies to them: a codec request must match a native
  rate and a native channel count exactly (else `-RIG_EINVAL`), it always
  opens as
  a native stream, and a codec-only caps entry is served verbatim — none
  of the widening below applies to it. This holds for any format outside
  the raw PCM/I-Q family masks, present or future.
- **Rates** (only when libsamplerate is built in — without it the
  effective rates equal the native rates): the native rates, plus the
  curated standard rates {8000, 11025, 16000, 22050, 24000, 44100,
  48000, 96000, 192000}, plus every exact integer division (factors
  2…10) of each native rate. Candidates outside libsamplerate's
  single-stage 1/256…256 conversion-ratio range are omitted. The result
  is deduplicated, ascending, and bounded by the largest native rate:
  audio up to and including it, I/Q strictly below it (an I/Q stream's
  sample rate is its represented bandwidth, so upsampling past the
  hardware cannot create information).
- **Channels:** the declared list is exact — the effective list equals
  the native list, and unlisted counts are never invented (a device may
  genuinely support only very specific counts, which is exactly what the
  list states). The single exception is the audio mono↔stereo map: when
  the hardware offers either count of the {1, 2} pair, the other becomes
  openable through the frontend's upmix/downmix. I/Q channels are
  coherent captures and never widen; a backend that can open a subset of
  its hardware channels declares each openable count explicitly.

**Session-scoped capabilities.** Some transports negotiate one stream
geometry for a whole connection — a single codec, rate and channel
count, fixed between `rig_open()` and `rig_close()` — so what *this
connection* can carry is a subset of what the *radio model* can do. A
backend publishes that subset from its `rig_open` hook through the
backend-internal `stream_set_session_caps()` (see the backend guide);
entries are copied, and publishing `NULL` restores the model
declaration. The mechanism is transparent to applications: both
discovery (`rig_stream_caps_count()` / `rig_stream_caps_at()`, and
rigctld's `\stream_caps`) and acceptance at `rig_stream_open()` follow
the session publication — the served view and the open-time resolution
derive from the same source, so they cannot disagree. The model
declaration in `rig_caps` is never modified (it is shared by every rig
of the model and is what `dump_caps` reports); the runtime state dump
(`--dump-caps` on a netrigctl rig) shows the session view. netrigctl
itself publishes the remote server's advertisement as session caps —
its whole streaming capability is per-connection.

**The libsamplerate dependency boundary.** Only sample-**rate** conversion
(resampling) depends on libsamplerate, which is optional at build time
(enabled by default; `--without-samplerate` omits it). Format conversion
and channel mapping are pure C built into the frontend and always
available, on every build. On a resampler-less build the only differences
are: the effective rate list equals the native rate list, and a
non-native rate request fails with `-RIG_EINVAL` — so
`RIG_STREAM_CONV_RATE` can never appear there, while
`RIG_STREAM_CONV_FORMAT` and `RIG_STREAM_CONV_CHANNELS` streams work
identically. For a remote rig (netrigctl, §6.1) it is the **server's**
build that matters — conversion runs there, and the effective rate list
the server advertises already reflects whether its resampler is present.

**Acceptance at `rig_stream_open()` is by rule, not by list membership** —
the advertised effective list is for discoverability. Any rate at or
below the largest native rate is accepted when the resampler is built and
the selected native/requested pair is within libsamplerate's single-stage
1/256…256 ratio range, listed or not (e.g. an arbitrary 22 222 Hz). The
frontend resolves the request to a native source: format prefers the float
format (lossless staging), then S16, then the lowest declared bit; the rate
source is the smallest native rate ≥ the request; and a conversion pipeline
is installed between that source and the stream. Requests beyond the rules
(an unsupported conversion ratio, a rate above the largest native rate, an
I/Q format on an audio stream, or more channels than the mapping allows)
fail with `-RIG_EINVAL`.

`rig_stream_get_conversions()` reports the installed stages
(`RIG_STREAM_CONV_FORMAT/RATE/CHANNELS`, 0 = native stream). A config
with `require_native = 1` refuses conversion with `-RIG_ENAVAIL`
(distinct from `-RIG_EINVAL` for the impossible). One exception to the
authoring rule: a relaying backend (netrigctl) fills the `native_*`
fields too, declaring both views pre-derived; the frontend then serves
the entry verbatim and delegates conversion to the far side (see §6.1).

### 3.4 Configuration (`struct rig_stream_config`)

```c
struct rig_stream_config {
    size_t              struct_size;        /* set by rig_stream_config_alloc() */
    rig_stream_type_t   type;
    rig_stream_format_t format;             /* a single format, not a bitmask */
    int                 sample_rate;
    int                 channels;
    int                 frame_samples;      /* samples/frame, 0 = backend default */
    size_t              buffer_bytes;       /* ring capacity bytes, 0 = default */
    unsigned int        buffer_duration_ms; /* if set and buffer_bytes==0, derive */
    unsigned int        time_stale_coarse_ms;     /* Staleness watchdog; 0 = rig/built-in default */
    unsigned int        time_stale_invalidate_ms; /* 0 = rig/built-in default */
    unsigned int        mtu;                /* sender path MTU, 0 = default 1500 (see 6.8) */
    unsigned int        transport_buffer_ms;         /* UDP socket buffer as ms of stream data; 0 = rig token / built-in 250 ms */
    unsigned int        transport_buffer_bytes;      /* explicit UDP socket buffer bytes; 0 = derive from transport_buffer_ms/rate */
    int                 require_native;     /* 1 = open only as a native stream:
                                             * -RIG_ENAVAIL rather than convert */
};
```

Allocate a config with `rig_stream_config_alloc()` and release it with
`rig_stream_config_free()` rather than declaring one on the stack — this keeps
it ABI-compatible as fields are appended (see "Object ownership & lifetimes"
in §4). All fields default to 0.

Default ring-buffer capacity when `buffer_bytes == 0` and
`buffer_duration_ms == 0`:

| Stream family | Default capacity |
|---------------|------------------|
| Audio (RX/TX) | 64 KB (`RIG_STREAM_AUDIO_BUF_DEFAULT`) |
| I/Q (RX/TX)   | 4 MB (`RIG_STREAM_IQ_BUF_DEFAULT`) |

The buffer capacity is rounded up to a power of two internally.

---

## 4. C API

The handle is opaque: `rig_stream_open()` returns a `rig_stream_t *` and
every later call takes that handle, so the internal representation can
evolve without breaking the ABI. All functions are
`HAMLIB_EXPORT`-declared in `include/hamlib/rig.h`.

```c
/* Capabilities (library-owned; the returned pointer is valid for the life of
 * the rig — see "Object ownership & lifetimes" below) */
int  rig_stream_caps_count(RIG *rig);
const struct rig_stream_caps *rig_stream_caps_at(RIG *rig, int index);

/* Configuration (library-allocated so it can grow ABI-compatibly) */
struct rig_stream_config *rig_stream_config_alloc(void);
void rig_stream_config_free(struct rig_stream_config *config);

/* Lifecycle */
int  rig_stream_open (RIG *rig, const struct rig_stream_config *config,
                      rig_stream_t **stream);
int  rig_stream_close(RIG *rig, rig_stream_t *stream);

/* Data exchange (blocking, timeout-based).
 * The trailing info parameter reports/carries per-call timing; pass NULL when timing is not needed. */
int  rig_stream_read (RIG *rig, rig_stream_t *stream, void *buffer,
                      size_t buffer_size, size_t *bytes_read, int timeout_ms,
                      struct rig_stream_read_info *info);
int  rig_stream_write(RIG *rig, rig_stream_t *stream, const void *buffer,
                      size_t buffer_size, size_t *bytes_written, int timeout_ms,
                      const struct rig_stream_write_info *info);
int  rig_stream_drain(RIG *rig, rig_stream_t *stream, int timeout_ms);

/* Accessors */
rig_stream_type_t rig_stream_get_type(const rig_stream_t *stream);
int  rig_stream_get_id(const rig_stream_t *stream);
int  rig_stream_get_max_payload(const rig_stream_t *stream);
int  rig_stream_get_conversions(const rig_stream_t *stream);
                                /* RIG_STREAM_CONV_* stages; 0 = native */

/* State control (independent operations) */
int  rig_stream_pause (RIG *rig, rig_stream_t *stream);
int  rig_stream_resume(RIG *rig, rig_stream_t *stream);
int  rig_stream_mute  (RIG *rig, rig_stream_t *stream);
int  rig_stream_unmute(RIG *rig, rig_stream_t *stream);

/* Health counters — one coherent snapshot */
int  rig_stream_get_stats(RIG *rig, rig_stream_t *stream,
                          struct rig_stream_stats *stats);

/* Async write-status events on a TX stream (blocking with timeout; see §8.9).
 * timeout_ms <0 blocks, 0 polls, >0 bounds; RIG_OK / -RIG_ETIMEOUT / -RIG_ENAVAIL */
int  rig_stream_wait_write_status(RIG *rig, rig_stream_t *stream,
                                  struct rig_stream_write_status *status,
                                  int timeout_ms);

/* Metadata */
int  rig_stream_read_metadata(RIG *rig, rig_stream_t *stream,
                                    struct rig_stream_metadata *meta);
int  rig_stream_write_metadata(RIG *rig, rig_stream_t *stream,
                               const struct rig_stream_metadata *meta);

/* Time model */
int      rig_stream_get_time_anchor  (rig_stream_t *stream,
                                      struct rig_stream_time_anchor *anchor);
int      rig_stream_get_hardware_time(RIG *rig, rig_stream_t *stream,
                                      struct rig_stream_time_anchor *now);
uint64_t rig_stream_get_samples_written  (const rig_stream_t *stream);
```

A minimal receive loop — open an audio RX stream, read it, close it:

```c
struct rig_stream_config *cfg = rig_stream_config_alloc();
rig_stream_t *stream;
float buf[4800];                 /* 50 ms of 48 kHz mono float samples */
size_t got;

cfg->type        = RIG_STREAM_TYPE_AUDIO_RX;
cfg->format      = RIG_STREAM_FORMAT_PCM_F32;
cfg->sample_rate = 48000;
cfg->channels    = 1;

if (rig_stream_open(rig, cfg, &stream) == RIG_OK)
{
    rig_stream_config_free(cfg);       /* the stream keeps its own copy */

    while (running)
    {
        if (rig_stream_read(rig, stream, buf, sizeof(buf), &got, 1000, NULL)
                == RIG_OK && got > 0)
        {
            process(buf, got / sizeof(float));
        }
    }

    rig_stream_close(rig, stream);     /* invalidates stream */
}
```

The config must come from `rig_stream_config_alloc()` — `rig_stream_open()`
rejects a stack-declared one (see "Object ownership & lifetimes"). Ask for a
format and rate from the effective caps; `rig_stream_caps_count()` and
`rig_stream_caps_at()` enumerate them (§3.3). After a successful open,
`rig_stream_get_conversions()` tells you whether the stream is served
natively or through conversion; set `cfg->require_native = 1` to demand
hardware-native service. Pass a `struct rig_stream_read_info *` as the last
argument instead of `NULL` when you need the producer sample index, drop
counts and capture time.

Semantics:

- **read/write** block up to `timeout_ms`, returning the number of bytes
  transferred via the out-parameter. A read timeout returns 0 bytes and
  increments the underrun counter. A non-NULL `info` additionally reports
  the producer sample index, drops, and capture time on read or
  carries the burst target on write.
- **Codec-frame streams** (compressed formats, e.g. `OPUS`) transport
  whole codec frames instead of a byte stream: `rig_stream_read()` returns
  **exactly one codec frame per call** — a buffer of
  `rig_stream_get_max_payload()` bytes always suffices, and an undersized
  buffer fails with `-RIG_EINVAL` consuming nothing.
  `read_info.sample_index` is the frame's decoded start index and
  `read_info.codec_frame_samples` its decoded duration (0 = unknown; on a
  remote rig the duration is not carried per packet, only the index).
  `rig_stream_write()` carries exactly one codec frame per call with the
  duration declared in `write_info.codec_frame_samples`, the frame must
  fit `rig_stream_get_max_payload()`, and a full TX ring **blocks** up to
  `timeout_ms` (returns `-RIG_ETIMEOUT`; the write side never drops or
  overwrites). RX mute discards frames (zeroed bytes are not a valid
  codec frame), TX mute drops writes, and
  `rig_stream_get_samples_written()` reports the decoded-sample position
  accumulated from the durations. On the RX producer edge (the backend,
  which cannot wait) a full ring drops the NEWEST frame, counted as an
  overrun with its duration in `dropped_samples_overrun` and the loss
  surfacing to the reader as a start-index jump.
- **drain** (TX) blocks until the ring buffer drains or the timeout
  elapses.
- **pause/resume** stop and restart backend device I/O. **mute/unmute**
  act at the ring-buffer level (RX writes discarded, TX reads return
  zeros). They are independent: a stream can be muted but not paused, and
  vice versa.
- **open** validates the config against the effective-set acceptance rules
  (§3.3), resolves the native source the backend will run at, and installs
  any conversion pipeline. It additionally rejects a non-positive
  `sample_rate` or `channels`, an unknown stream type, a `struct_size` of 0,
  a rate conversion outside libsamplerate's supported ratio, and exhaustion
  of either the caps `max_streams` limit or the `HAMLIB_MAX_STREAMS` slots for
  that type — all with `-RIG_EINVAL`. A convertible request with
  `require_native = 1` fails with `-RIG_ENAVAIL`.
- **close** unregisters the stream, wakes any blocked reader or
  write-status waiter, then waits for every `rig_stream_*` call already in
  flight on that stream to return before the handle is torn down. A
  concurrent call therefore either completes normally or fails cleanly —
  it can never touch a freed stream. The handle is invalid once close
  returns.
- **Thread safety:** a single consumer per stream. Concurrent reads on the
  same stream split data unpredictably — the ring-buffer lock prevents
  corruption, not logical interleaving.

### Object ownership & lifetimes

Every public streaming struct is designed so it can gain fields in a later
release **without breaking the ABI**. The mechanism follows one rule — *who
allocates the storage?*

- **Library-allocated, opaque** — the stream handle `rig_stream_t`. Obtained
  from `rig_stream_open()`, released by `rig_stream_close()`. The layout is
  private and can change freely.
- **Library-allocated, durable** — `rig_stream_config`. Applications MUST get
  one from `rig_stream_config_alloc()` (zeroed; `0` == default everywhere) and
  release it with `rig_stream_config_free()` rather than declaring one on the
  stack. Because the library allocates it, the buffer is always sized to the
  library's own view of the struct, so appended fields (e.g. `mtu`) stay
  ABI-safe. `rig_stream_config_alloc()` stamps `struct_size`, and
  `rig_stream_open()` **rejects a config with `struct_size == 0`** — so a
  stack-declared config fails fast instead of corrupting memory. The config may
  be freed as soon as `rig_stream_open()` returns — the stream keeps its own copy.
- **Library-owned, immutable (borrow)** — `rig_stream_caps`, reached through
  `rig_stream_caps_count()` / `rig_stream_caps_at()`. The returned pointer is
  owned by the library and valid **for the life of the rig**; its contents are
  immutable. The caller MUST NOT free it and copies fields out to retain them.
  No caps array is allocated by the application, so the descriptor may grow
  freely.
- **Caller-allocated (out-param + reserved tail)** — `rig_stream_stats`,
  `rig_stream_metadata`, `rig_stream_time_anchor`, `rig_stream_read_info`,
  `rig_stream_write_info`. These are filled on the caller's stack, which gives
  each (possibly cross-thread) poller an isolated snapshot. Each carries a
  trailing `uint64_t _reserved[]` block for future fields; a library filler
  zeroes it, and callers **SHOULD** zero-initialize the struct (`= {0}`) so a
  not-yet-known field reads as `0` even against an older library.

**Reserved-space carve policy.** Caller-allocated structs never change
size: each keeps exactly **one** trailing `_reserved` array, and a new
field first fills any padding hole in the existing layout (e.g.
`codec_frame_samples` sits in former tail padding of both info structs),
then carves whole slots from the **front** of the reserved array —
shrinking the array, never splitting it into multiple reserved fields.
`rig_stream_caps` (library-owned) additionally appends array-sized fields
with a tail-only reserve (`uint32_t _reserved32[1]` +
`uint64_t _reserved[4]`): fields are never appended after it, because
`rig_caps.stream_caps` is a publicly indexable array and its element
`sizeof` is therefore frozen for the ABI major — future scalars carve
reserve slots instead (32-bit from `_reserved32`, 64-bit from
`_reserved`). `rig_stream_config` needs no reserved tail at all — the
allocator plus `struct_size` make plain appending safe. Flag headroom
lives inside the fields themselves (`caps_flags` is a 64-bit namespace
with 60 free bits, metadata `field_mask` bits ≥ `1<<4`, time-flag bits
5–7) and the wire protocol's extension seams are listed in §6.2.1.

### Health counters

Stream health is returned as one snapshot — per-cause event counts plus
lost-sample totals, so loss *ratios* by cause are directly computable:

```c
struct rig_stream_stats {
    /* event counts (local ring) */
    uint32_t overruns;      /* local ring full on write; oldest overwritten */
    uint32_t underruns;     /* local blocking read timed out empty —
                               counted only once the producer has ever
                               delivered (startup silence is not an
                               underrun) */
    uint32_t gaps;          /* radio/network-side gaps marked by backend */
    uint32_t gaps_unknown;  /* subset of gaps with unknown size */
    uint32_t link_loss;     /* network client only: app-link UDP loss */
    uint32_t tx_late;       /* timed TX bursts that missed their slot */
    /* remote (server-reported) event counts, network client only */
    uint32_t remote_overruns;    /* server TX ring overrun / RX overrun-replay */
    uint32_t remote_underruns;   /* server TX ring underrun */
    uint32_t write_events_dropped;  /* write-status events dropped on FIFO overflow */
    /* lost-sample totals (per cause) */
    uint64_t dropped_samples_gap;     /* lower bound if gaps_unknown > 0 */
    uint64_t dropped_samples_overrun;
    uint64_t dropped_samples_link;    /* network client only */
    uint64_t codec_frames;            /* whole codec frames produced (0 = raw
                                         stream); today equal to the datagram
                                         count (one frame per datagram) but
                                         counts FRAMES, surviving any future
                                         packing */
};
```

On a direct stream the `link_*` and `remote_*` fields stay 0. `overruns` /
`underruns` count **local** ring events; on a netrigctl client stream a
server-reported TX under/overrun is counted separately in `remote_overruns` /
`remote_underruns` (delivered by the `WRITE_STATUS` frame, §8.9), so local and
remote causes stay distinguishable. The server-only view remains queryable via
`\stream_status` (see loss classification).

The overflow policy for raw streams is **overwrite-oldest**: a full ring
buffer never blocks the producer; the oldest unread bytes are dropped and
`overruns` increments. This keeps the stream current (most-recent data
wins), which is the right trade-off for real-time audio/I/Q. Codec-frame
streams instead **drop the newest** incoming frame on the RX producer edge
(overwriting would destroy frame alignment; see §4), and the blocking
`rig_stream_write()` never drops at all.

---

## 5. Backend interface

A backend implements at minimum `stream_open` and `stream_close`. All
other hooks are optional; when absent, the frontend uses the ring buffer
directly. Function pointers in `struct rig_caps`:

```c
int (*stream_open) (RIG *rig, struct rig_stream *stream);
int (*stream_close)(RIG *rig, struct rig_stream *stream);

/* Optional — wrap the ring buffer instead of using it directly.
 * The info parameter mirrors the public API; default-path
 * backends ignore it — the frontend fills/consumes it. */
int (*stream_read) (RIG *rig, struct rig_stream *stream, void *buffer,
                    size_t buffer_size, size_t *bytes_read,  int timeout_ms,
                    struct rig_stream_read_info *info);
int (*stream_write)(RIG *rig, struct rig_stream *stream, const void *buffer,
                    size_t buffer_size, size_t *bytes_written, int timeout_ms,
                    const struct rig_stream_write_info *info);
/* Optional — flush a hardware TX FIFO; if NULL the frontend polls the
 * ring buffer empty. */
int (*stream_drain)(RIG *rig, struct rig_stream *stream, int timeout_ms);

/* Optional — hardware-level pause/resume; if NULL the frontend flag suffices */
int (*stream_pause) (RIG *rig, struct rig_stream *stream);
int (*stream_resume)(RIG *rig, struct rig_stream *stream);

/* Optional — timed TX metadata; if NULL the frontend calls rig_set_freq() etc. */
int (*stream_apply_metadata)(RIG *rig, struct rig_stream *stream,
                             const struct rig_stream_metadata *meta);

/* Optional — radio sample-clock timestamp; if NULL the frontend returns
 * host CLOCK_REALTIME (RIG_STREAM_TIME_SRC_HOST). */
int (*stream_hardware_time)(RIG *rig, struct rig_stream *stream,
                            struct rig_stream_time_anchor *now);
```

The backend always runs at the **native** side of any conversion: it reads
its operating format, rate and channel count from `stream->backend_config`
(equal to `stream->config` on a native stream) and never sees the
application's requested config. An RX producer delivers its native bytes
with `stream_backend_write(stream, data, len)` — the frontend applies the
conversion pipeline when one is installed and writes the ring, or writes
the ring directly on a native stream. A TX consumer reads the ring as
before: the ring always holds the backend-native format, because
conversion runs on the producer side of the ring in both directions
(`rig_stream_write()` converts before enqueueing). Backend-domain sample
counts (time anchors, gap accounting) are rescaled by the frontend.

The underlying internal `stream_ringbuf_*` API:

| Function | Behavior |
|----------|----------|
| `stream_ringbuf_write(rb, data, len)` | Never blocks; overwrites oldest if full, bumps overruns. |
| `stream_ringbuf_read(rb, data, len, timeout_ms)` | Blocks up to timeout; returns bytes read. A timeout bumps underruns only once the ring has ever been fed (startup silence is not an underrun). |
| `stream_ringbuf_available(rb)` | Bytes currently readable. |
| `stream_ringbuf_reset(rb)` | Discard buffered data. |

`stream_ringbuf_init`/`stream_ringbuf_destroy` are frontend-managed — backends do not
call them. RX producers should prefer `stream_backend_write()` over raw
`stream_ringbuf_write()` so converted streams work unchanged.

**Codec-frame streams** have their own produce/consume pair — the ring
holds length-prefixed codec-frame records, never raw bytes:

| Function | Behavior |
|----------|----------|
| `stream_backend_write_frame(stream, buf, len, duration_samples)` | RX produce: enqueue ONE codec frame with its decoded duration (0 = unknown). Never blocks; a full ring drops the NEWEST frame with overrun accounting. Frame start indexes accumulate from the durations. |
| `stream_backend_write_frame_indexed(...)` | Same, with an explicit start index (relaying producers that know the absolute position — e.g. the netrigctl client stamping the wire timestamp). |
| `stream_backend_read_frame(stream, buf, cap, &len, &duration, &start_index, timeout_ms)` | TX consume: dequeue ONE whole codec frame with its metadata; an undersized cap fails without consuming. |

The producer supplies durations — from a fixed protocol cadence (e.g.
FlexRadio's 10 ms Opus frames = 240 samples at 24 kHz) or the codec's own
in-band self-description — so the core stays codec-ignorant. Radio-side
gap marks (`rig_stream_mark_gap()`) advance the frame index so losses
surface to consumers as start-index jumps.

### Threading model

The framework is agnostic to the exact threading shape; the contract is
only "the backend keeps the ring buffer fed (RX) or drained (TX)." Two
common shapes:

- **One I/O thread per stream** — simplest; each RX stream owns a
  producer thread, each TX stream a consumer thread. This is what the
  dummy backend uses.
- **Shared device thread with demux** — a single socket/thread fans data
  out to multiple per-stream ring buffers. Appropriate when the radio
  multiplexes all streams on one transport.

Per-stream backend state is allocated in `stream_open`, stored in
`stream->backend_priv` (the one handle field the backend owns), and freed
in `stream_close`. The frontend exposes `stream->config`,
`stream->ringbuf`, `stream->type`, `stream->paused`/`muted`/`active`
(`HAMLIB_ATOMIC int`), `stream->center_freq`, `stream->vfo`, and
`stream->gap_count` (the running sample count is available via
`rig_stream_get_samples_written()`).

### Time model

A backend that supports the time model additionally:

- pushes capture-time anchors via `rig_stream_push_time_anchor()` — at
  stream start, at least every 1 s, and (flagged DISCONTINUITY) right after
  any detected gap;
- reports radio/network-side losses via
  `rig_stream_mark_gap(stream, dropped_samples)` (0 = size unknown) **before**
  writing the post-gap data — this advances the sample-index domain
  without writing bytes so the hole reaches the app exactly like an
  overrun (see loss classification);
- for timed TX, drains pending burst targets with
  `rig_stream_pop_tx_target()` from its TX thread and gates emission
  (see timed transmit);
- declares what it can do via `caps_flags`
  (`RIG_STREAM_CAP_TIMED_TX_COARSE/_SAMPLE/_BURST_PTT/_HW_TIME`) and
  `tx_schedule_horizon_ms`.

Both functions are backend-facing (they take the internal
`struct rig_stream *`).

Direction/type classification helpers:
`stream_type_is_rx()`, `stream_type_is_tx()`, `stream_type_is_iq()`.

---

## 6. rigctld UDP streaming

When a backend works through the C API, rigctld streaming works for free:
rigctld calls the same `rig_stream_open/close` hooks and feeds sample
data to remote clients over UDP. The control channel is the existing
rigctld TCP command port; sample data flows on a per-stream UDP socket.

### 6.1 TCP text commands

Eleven commands (codes `0xb0`–`0xba`). **All require the extended-response
`+` prefix** (long-form names with `\`), because the high command codes
exceed the signed-char range used by the short-form parser:

| Command | Code | Purpose |
|---------|:----:|---------|
| `\stream_caps`         | 0xb0 | List backend stream capabilities |
| `\stream_open`         | 0xb1 | Open a stream, allocate UDP socket |
| `\stream_close`        | 0xb2 | Close a stream |
| `\stream_status`       | 0xb3 | Per-stream counters (packets, per-cause losses, overruns, underruns, tx_late, latest time anchor) |
| `\stream_pause`        | 0xb4 | Pause backend I/O |
| `\stream_resume`       | 0xb5 | Resume backend I/O |
| `\stream_mute`         | 0xb6 | Mute (discard RX / zero TX) |
| `\stream_unmute`       | 0xb7 | Unmute |
| `\stream_metadata_read` | 0xb8 | Read latest metadata |
| `\stream_drain`        | 0xb9 | Drain TX buffer |
| `\stream_list`         | 0xba | List all active streams |

`\stream_open` syntax (positional, then optional `key=value`):

```
+\stream_open <TYPE> <FORMAT> <RATE> [key=value ...]
   TYPE   = AUDIO_RX | AUDIO_TX | IQ_RX | IQ_TX
   FORMAT = PCM_S8|PCM_U8|PCM_S16|PCM_F32|
            IQ_CS8|IQ_CU8|IQ_CS16|IQ_CF32
   RATE   = sample rate in Hz (0 < rate <= 20000000)

   key=value options:
     metadata_interval=<ms>   change-poll interval, clamped 25..1000 (0 = default 100)
     metadata_refresh=<ms>    unconditional refresh in ms of stream data
                              (default 100; 0 = every data packet)
     transport_buffer_ms=<ms>          socket buffer as ms of stream data (default 250)
     transport_buffer_bytes=<n>        socket buffer size override (0 = derive from rate)
     mtu=<bytes>              sender path MTU for datagram sizing (0 = default
                              1500; clamped to [576, 9216], see 6.8)
     multicast=<ADDR:PORT>    RX only, multicast group (see 6.4)
     ttl=<1..255>             multicast TTL/hops

     channels=<1..255>        stream channel count (default 1; validated
                              against the caps entry — coherent I/Q may
                              exceed stereo)
     require_native=<0|1>     1 = open only as a hardware-native stream;
                              a convertible request is refused with
                              RPRT -RIG_ENAVAIL (default 0 = convert)
     time_stale_coarse=<ms>   staleness → COARSE threshold (see details below)
     time_stale_invalidate=<ms> staleness → time-invalid threshold (see details below)
     keepalive_timeout=<s>    silence before the client is dropped, clamped
                              5..3600 (0 = default 30; see 6.3)
```

Without the `channels=` key the command opens with channels = 1, so a
stereo-only backend needs `channels=2` for a correctly-labeled stream.

`\stream_open` response fields (verbose ext_resp form):

```
stream_id: <id>
source_id: <stream source ID>  0 = unset (see §6.2.1)
udp_port: <port>
subscribe_token: <32-bit token>
max_payload: <bytes>         effective frame-aligned payload budget (see 6.8)
conversions: <C1,C2,...>     server-side conversion stages, comma-
                             separated RIG_STREAM_CONV_* names with the
                             prefix stripped (FORMAT, RATE, CHANNELS);
                             empty = native stream
[multicast: <addr>]          (only for multicast streams)
```

The client uses `udp_port` and `subscribe_token` to drive the UDP session
(see subscribe handshake and keepalive later in this document), and
`max_payload` to size its own TX packetization to the negotiated path.

`\stream_caps` emits one line per capability entry with both views —
effective sets first, then the hardware-native view. The entries answer
for the **current session**: a backend that negotiates capabilities per
connection (§3.3, session-scoped capabilities) advertises what this
connection can carry right now, not the model's full range:

```
type=<TYPE> formats=<F1,F2,...> rates=<R1,R2,...> channels=<C1,C2,...>
max_streams=<n> flags=<G1,G2,...> tx_horizon_ms=<n>
native_formats=<...> native_rates=<...> native_channels=<C1,C2,...>
```

(one line per entry; wrapped here for readability). `flags=` carries the
`RIG_STREAM_CAP_*` names with the prefix stripped (`TIMED_TX_COARSE`,
`TIMED_TX_SAMPLE`, `BURST_PTT`, `HW_TIME`) and `tx_horizon_ms=` the
timed-TX lead-time limit (0 = not schedulable). This line is the **one
canonical textual rendering** of a capability entry — it carries every
defined field of `struct rig_stream_caps`, and the same format (minus
the `native_*` keys, which a bare declaration does not have) is what
`dump_caps` prints in its streaming section, so one parser serves both.
A parser MUST skip unknown keys and unknown flag names: future
capability flags extend the `flags=` list without a new key.

Every key uses one of
three value syntaxes, and each key always uses the same one:

- **name list** — comma-separated symbolic names, no spaces
  (`formats=PCM_S16,PCM_F32`). One name is a one-element list; a list
  may be empty (`flags=` on an entry with no capability flags, like
  `conversions:` on a native stream).
- **integer list** — comma-separated decimal integers, ascending, no
  spaces (`rates=24000,48000`, `channels=1,2`). Ranges are **not** part of
  the grammar — a contiguous set is spelled out (`channels=1,2,3,4`).
- **scalar** — a single decimal integer (`max_streams=4`,
  `tx_horizon_ms=30000`).

The effective sets are
what `\stream_open` serves — through server-side conversion where they
exceed the native sets. The netrigctl client backend parses both views and
relays them to its application verbatim (pre-derived caps; the server's
advertisement is authoritative because the conversion runs there), and
reports the server's `conversions:` value through
`rig_stream_get_conversions()`. For such a relayed rig, acceptance at the
client is **membership in the advertised effective sets** rather than the
local rule-based acceptance of §3.3 — rule-based acceptance of arbitrary
rates applies where the conversion actually runs, so a text-protocol
client talking straight to rigctld may open unlisted rule-acceptable
rates, while a netrigctl application chooses from the advertised list. An
older server without the `native_*` keys is still accepted — the client
falls back to deriving the effective view locally — and an older client
simply ignores the added keys and response lines.

**The remaining commands.** Every command below takes the numeric
`stream_id` returned by `\stream_open`. Streams are owned by the TCP
connection that opened them: commands against another connection's
stream fail with `RPRT` -RIG_EACCESS (`\stream_list` is the exception —
it reports all clients' streams). In the extended response protocol
every response opens with an echo line (`stream_status: 1`), then any
`key: value` fields joined by the chosen separator (`+` = newline), then
the `RPRT <code>` result (0 = success; a negative code is the RIG_E*
error). Commands with no output fields answer with the echo line and
`RPRT` alone:

- `\stream_close <stream_id>` — stop the feeder, close the stream's
  UDP socket and release the stream.
- `\stream_pause <stream_id>` / `\stream_resume <stream_id>` —
  suspend and restart backend I/O without releasing anything; a paused
  stream keeps its UDP socket, subscription and counters.
- `\stream_mute <stream_id>` / `\stream_unmute <stream_id>` — keep
  the stream flowing but discard RX payloads / substitute silence on TX.
- `\stream_drain <stream_id>` — block until the TX ring has been
  played out, bounded at 1 second.

`\stream_status <stream_id>` response fields, in order (scalars all
decimal except `time_flags`):

```
type: <TYPE>                 stream type name
stream_id: <n>
sample_rate: <hz>            the opened (client-side) configuration
format: <FORMAT>
channels: <n>
udp_port: <port>
paused: <0|1>
muted: <0|1>
conversions: <C1,C2,...>     server-side conversion stages (see
                             stream_open; empty = native stream)
codec_frames: <n>            whole codec frames produced (0 = raw stream)
packet_count: <n>            UDP datagrams sent (RX) / accepted (TX)
gap_count: <n>               inbound datagrams missing by sequence
                             (meaningful on TX streams)
overruns: <n>                ring health counters (§4)
underruns: <n>
backend_gaps: <n>            backend-reported sample gaps
backend_gaps_unknown: <n>    gaps of unknown length
link_loss: <n>               app-link UDP loss events (netrigctl
                             client side; 0 in rigctld itself)
tx_late: <n>                 timed-TX deadline misses
dropped_samples_gap: <n>     per-cause dropped-sample totals
dropped_samples_overrun: <n>
dropped_samples_link: <n>
time_anchor_index: <n>       latest time anchor (§8); these five appear
time_anchor_seconds: <s>     only once the stream has an anchor
time_source: <n>
time_flags: 0x<hex>
time_accuracy: <n>
multicast: <addr>            multicast streams only
ttl: <n>
```

`\stream_metadata_read <stream_id>` — the latest metadata snapshot
(§7):

```
center_freq: <hz>
vfo_freq: <hz>
vfo_id: <n>
ptt: <0|1>
field_mask: <bits>           which fields carry valid data (§7)
sample_index: <n>            stream position the values belong to
```

`\stream_list` — one block per active stream, all clients, blank-line
separated:

```
stream_id: <n>
source_id: <n>
type: <TYPE>
format: <FORMAT>
sample_rate: <hz>
channels: <n>
udp_port: <port>             0 unless the caller owns the stream
paused: <0|1>
muted: <0|1>
owner: <0|1>                 1 = opened by this TCP connection
conversions: <C1,C2,...>     (empty = native stream)
codec_frames: <n>
```

**A complete session** against the dummy rig (`rigctld -m 1`), captured
verbatim (`>` marks client input). The dummy's native audio format is
PCM_F32, so this PCM_S16 open is served through server-side format
conversion — visible as `conversions: FORMAT` in the open response and
everywhere after (a native stream shows an empty value):

```
> +\stream_open AUDIO_RX PCM_S16 48000 channels=2
stream_open: AUDIO_RX PCM_S16 48000
stream_id: 1
source_id: 11300
udp_port: 53758
subscribe_token: 2341159422
max_payload: 1420
conversions: FORMAT
RPRT 0
> +\stream_status 1
stream_status: 1
type: AUDIO_RX
stream_id: 1
sample_rate: 48000
format: PCM_S16
channels: 2
udp_port: 53758
paused: 0
muted: 0
conversions: FORMAT
codec_frames: 0
packet_count: 0
gap_count: 0
overruns: 9
underruns: 0
backend_gaps: 0
backend_gaps_unknown: 0
link_loss: 0
tx_late: 0
dropped_samples_gap: 0
dropped_samples_overrun: 0
dropped_samples_link: 0
time_anchor_index: 12000
time_anchor_seconds: 1786606908
time_source: 1
time_flags: 0x00
time_accuracy: 2
RPRT 0
> +\stream_list
stream_list:
stream_id: 1
source_id: 11300
type: AUDIO_RX
format: PCM_S16
sample_rate: 48000
channels: 2
udp_port: 53758
paused: 0
muted: 0
owner: 1
conversions: FORMAT
codec_frames: 0
RPRT 0
> +\stream_close 1
stream_close: 1
RPRT 0
```

(The non-zero `overruns` is real and expected here: no UDP subscriber
was attached in this capture, so the RX ring overwrote unread data — a
real client attaches with the subscribe handshake of §6.3 and drains the
stream.)

The network stream commands only function inside rigctld daemon;
the same commands invoked through local rigctl return `-RIG_ENAVAIL`.

### 6.2 UDP wire protocol

This section is **normative**. MUST / MUST NOT / SHOULD / MAY are used in the
RFC 2119 sense. A conforming implementation of wire version 1 obeys every MUST
below; the reserved ranges are how the format grows without breaking existing
peers.

Every UDP datagram begins with a fixed **32-byte header, big-endian**
(`struct rig_stream_packet_header`), optionally followed by a payload. The
header, the metadata block (§7) and the time block are big-endian (network
byte order); the **sample payload is little-endian**.

(The sample converters process samples in host byte order and do not swap, so
the implementation currently assumes a little-endian host; a big-endian build
fails at compile time — a `WORDS_BIGENDIAN` guard in `stream_convert.c` —
rather than emitting corrupt samples. That is a limitation of the current
build, not of the wire format, whose endianness is defined above.)

```
Offset Size Field             Notes
  0     1   version           RIG_STREAM_PROTOCOL_VERSION = 1
  1     1   type              rig_stream_type_t
  2     2   stream_id
  4     4   subscribe_token   anti-hijack token (see security below)
  8     4   seq               wrapping sequence number (32-bit)
 12     8   timestamp         sample count since stream start (64-bit)
 20     4   sample_rate       sample rate in Hz
 24     1   format            format ID (see table below)
 25     1   channels          channel count (uint8, 1..255)
 26     2   source_id         stream source ID; 0 = unset (see §6.2.1)
 28     2   control           control bits (see table below)
 30     2   payload_len       payload length in bytes, following the header
```

**Version (normative).** A receiver **MUST** drop, and SHOULD log, any datagram
whose `version` differs from the version it implements. This is the evolution
lever: a future version-2 sender fails safe against a version-1 receiver
instead of being misparsed.

**Stream source ID (offset 26–27).** A 16-bit identifier of the **signal
source as published** — one device (rig), as published by one daemon — stamped
on every server frame of the device's streams. `0` means unset: identity falls
back to the source transport tuple. Semantics, assignment, and direction rules
are normative in §6.2.1.

The header carries a compact 1-byte **format ID** (not the bitmask):

| ID | Format    | ID | Format     |
|:--:|-----------|:--:|------------|
| 0 | PCM_S8     | 5 | IQ_CS8     |
| 1 | PCM_U8     | 6 | IQ_CU8     |
| 2 | PCM_S16    | 7 | IQ_CS16    |
| 3 | PCM_F32    | 8 | IQ_CF32    |
| 4 | OPUS       | 0xFF | invalid |

IDs 0–8 are assigned and `0xFF` is invalid; IDs **9–254 are reserved**. A
receiver **MUST** treat a reserved or unadvertised format ID as an error.
`rig_stream_open` rejects any format the backend's caps do not advertise (e.g.
`OPUS` on a PCM-only rig).

`control` bits:

| Bit    | Name           | Direction | Meaning |
|:------:|----------------|-----------|---------|
| 0x0001 | PING           | client → server | keepalive |
| 0x0002 | PONG           | server → client | keepalive reply |
| 0x0004 | SUBSCRIBE      | client → server | RX subscribe request |
| 0x0008 | SUBSCRIBE_ACK  | server → client | subscription acknowledged |
| 0x0010 | ERROR          | server → client | payload is an error frame |
| 0x0020 | TIME           | either | payload begins with a 20-byte time block |
| 0x0040 | METADATA       | either | payload is a metadata frame, not samples |
| 0x0080 | WRITE_STATUS   | server → client | payload is a 36-byte write-status block |

Bits are grouped by role: keepalive pair, handshake pair, then the
payload-affecting bits, with each request/reply pair on adjacent bits. Bits
**`0x0100`–`0x8000` are reserved** and MUST be sent as zero; a receiver **MUST**
drop a control frame carrying bits it does not recognize rather than ingest it
as data. `ERROR` is reserved for a server→client error frame, but **version 1
defines no ERROR payload format** — a receiver treats an ERROR frame as a
no-op drop. `TIME` is valid only on a data frame or a time-only frame.
`WRITE_STATUS` reports an async TX under/overrun or late burst back to a TX
client (see §8.9); it carries its own payload and draws a `seq` value.

Data, metadata, and time-only frames all draw from the **same `seq`
counter**, so a receiver detecting a `seq` gap must account for
interspersed non-data (metadata / time-only) frames before inferring
lost samples. Header-only control replies — `SUBSCRIBE_ACK` and `PONG` —
are sent with `seq = 0` (they do not consume the stream counter) and are
excluded from gap accounting by the client.

**Payload length (normative, security).** A receiver **MUST** reject any
datagram whose `payload_len` exceeds `datagram_length − 32` before reading the
payload, and each embedded sub-block (metadata, time) **MUST** be length-checked
against the remaining payload before it is parsed. This keeps every payload
read inside the received datagram.

**Metadata `field_mask` (normative).** Bits 0–3 are assigned (`VFO`, `PTT`,
`IQ_CENTER`, `VFO_FREQ`; see §7); bits **`≥ 1<<4` are reserved**. The block
grows **append-only** at fixed offsets; a receiver sizes it via `payload_len`
and **MUST ignore unknown trailing bytes**.

**Datagram size.** The default payload budget is **1420 bytes** (1500 MTU − 40
IPv6 − 8 UDP − 32 header, rounded down to a whole frame). A sender MAY raise its
path MTU (`rig_stream_config.mtu`, or the `mtu=` `\stream_open` key), clamped to
`[576, RIG_STREAM_MAX_DATAGRAM]` with **`RIG_STREAM_MAX_DATAGRAM` = 9216**.
Every receiver **MUST** accept a datagram up to `RIG_STREAM_MAX_DATAGRAM`; this
is the version-1 interoperability ceiling that lets a larger-MTU sender work
with an existing receiver without a version bump (§6.8). A frame (sample ×
channels) **MUST NOT** span two datagrams.

**Byte order and versioning.** The header / metadata / time are big-endian, the
sample payload is little-endian; the header is 32 bytes; `channels` is a `uint8` (1–255),
channel-interleaved; `RIG_STREAM_MAX_DATAGRAM` is the receiver ceiling.
Changing any of these requires a `version` bump.

### 6.2.1 v1 semantic contracts (normative, frozen)

Beyond field *layout*, these *meanings* are frozen at v1 — a later append can
add a field but cannot redefine what an already-shipped byte means, so both
sides MUST assume the defaults below.

**Sample full-scale.** The nominal full-scale amplitude per format is:
`PCM_F32` / `IQ_CF32` = ±1.0; `PCM_S16` / `IQ_CS16` = ±32767;
`PCM_S8` / `IQ_CS8` = ±127; `PCM_U8` / `IQ_CU8` = 0..255 with 128 = zero. A
future dBFS/dBm calibration (reference-level) metadata field is defined against
this anchor.

**I/Q spectral sense.** The canonical sense is **non-inverted**: a positive
baseband frequency maps to an RF frequency **above** `center_freq`, and Q
**leads** I by 90° for a positive frequency. `field_mask` bit 4
(reserved, `SPECTRAL_INVERSION`) MAY later flag an inverted window; **its
absence MUST be read as non-inverted**.

**Frequency reference plane.** `center_freq` and `vfo_freq` are the radio's
**reported RF (dial)** frequency — **not** transverter/converter-corrected
antenna RF. A future `field_mask` bit MAY carry an `RF_OFFSET` for antenna-plane
reconstruction; until then a consumer behind a transverter applies the offset
out-of-band.

**Coherent channels.** The `channels` of one stream are **coherent** (shared
LO / sample clock), **interleaved per sample**, and share one `sample_rate`,
`format`, and `center_freq`. Independent-frequency or independent-clock windows
MUST be separate streams. Appended metadata fields are fixed-size except at most
one terminal variable-length field (reserved for a future per-channel-frequency
vector), so fixed offsets are preserved.

**Compressed formats.** For a `frame_bytes == 0` (compressed) format such as
`OPUS` (implemented as codec-frame passthrough): `payload_len` is the exact
codec-frame byte count; the header `timestamp` is the decoded output-sample
index of the frame's FIRST sample in the header's `sample_rate` domain,
advancing by each frame's decoded duration; **exactly one codec frame
occupies one datagram** (the "a frame never spans a datagram" rule applies to
the *codec* frame), and a relay MUST NOT split or merge codec frames. The
per-frame decoded duration is NOT carried on the wire: the producer knows it
(fixed cadence, or the codec's own in-band self-description such as the Opus
TOC byte), and a receiver that needs it derives it from consecutive
timestamps. A decoder needs no out-of-band per-packet codec parameters;
encoder parameters (bitrate, frame size) are negotiated on the control plane
(config / rigctld `\stream_open key=value`). Deployed example of this exact
model: FlexRadio's radio-side Opus — fixed 10 ms stereo frames at 24 kHz, one
frame per packet.

**Stream identity & multicast.** When `source_id` is zero, a receiver MUST
treat the tuple `(source IP, source UDP port, stream_id)` as the canonical
stream identity and demultiplex on it — **not** on `stream_id` alone
(independent servers each number `stream_id` from 0). When `source_id` is
non-zero, the path-independent identity is **`(source_id, stream_id)`**;
unicast receivers MAY keep using the tuple (their per-stream socket already
demultiplexes), and multicast receivers SHOULD prefer `(source_id,
stream_id)`. The `subscribe_token` is **unicast** authentication only: on a
multicast group it is a fixed/ignored field, reception requires no SUBSCRIBE
handshake, and a receiver MUST NOT drop group datagrams on a token mismatch
(this keeps handshake-free multicast late-join valid).

**Stream source ID.** `source_id` names the **signal source as published**:
one device, as published by one daemon. Each device maps to exactly one
`source_id` per publishing daemon and all of that device's streams carry it on
every server frame (data, metadata, time-only, control). `stream_id` MUST be
unique within a `source_id`. A daemon MAY publish several `source_id`s (one
per device), and one radio published by two daemons is two sources — they MUST
use distinct IDs. The field is meaningful only in the published (server →
client / multicast) direction: a client MUST send `0`, and the server MUST
drop inbound frames carrying a non-zero `source_id` (reject rather than
ingest, like unknown control bits). Assignment: the per-device conf token
`stream_source_id` (rigctld convenience alias `--stream-source-id`) sets an
explicit value — convention `0x0001`–`0x0FFF` for manual assignment, not
enforced; when unconfigured, a stable ID is **derived** from static
configuration (FNV-1a over `hostname|listen_port|model_id|pathname`, folded
into `0x1000`–`0xFFFF` so derived IDs never collide with manual ones) and
therefore survives daemon restarts while the deployment configuration is
unchanged. Setting the token to `0` forces unset (tuple identity). Uniqueness
within a deployment is an operator responsibility; a receiver SHOULD warn when
one `source_id` arrives concurrently from two different source tuples. The
effective value is reported by `\stream_open` and `\stream_list`.

**Time base.** `seconds` in the TIME block and time anchors is POSIX
`CLOCK_REALTIME` (UTC, Unix epoch); leap seconds are handled by the host clock
(smear or step). A leap-second straddle is flagged by `DISCONTINUITY` together
with time-flags **bit 6** (reserved leap marker). A future timescale selector
(to advertise GPS/TAI/monotonic time) MAY be added via a reserved time-flag value
or a reserved `rig_stream_time_anchor` byte; until then all absolute time is UTC.

**Reserved namespaces & extension seams.** Additions use these seams so v1
receivers keep working:
- **Control bits `0x0100`–`0x8000`** (8 free) — new frame types / per-data-packet
  flags. Current receivers **ignore** unknown control bits, so these are usable
  on data frames. Earmarked (reserved, not yet defined): a front-end
  **over-range/clip** flag, a **fragment-continues** flag (reassembly via `seq`),
  an RTCP-style **receiver-report** frame, and a **security key-agreement** frame.
  (`0x0080` WRITE_STATUS is now assigned; see §8.9.)
- ~~Header bytes 30–31~~ — the former reserved tail is consumed by the
  `source_id` field (offsets 26–31 shifted); no reserved header bytes remain.
  New per-packet needs use a control bit or an appended payload block.
- **Metadata `field_mask` bits `>= 1<<4`** — reserved; the block grows
  append-only at fixed offsets, sized by `payload_len`.
- **Format IDs 9–254** — reserved. The binding ceiling is the 32-bit
  `caps.formats` bitmask (23 bits free), not the 255-wide ID space; beyond that a
  `formats2` caps word is the escape.
- **Security posture.** No in-header cipher/flags field (SRTP shows it is
  unnecessary; `seq` is the replay counter). Confidentiality/integrity is added
  later via a transport wrapper (VPN/DTLS today) or a `version`-bump auth trailer
  with keying negotiated on the control plane.

### 6.3 Subscribe handshake and keepalive

Unicast RX streams use a subscribe handshake so the server learns the
client's UDP address and validates it:

```
client → SUBSCRIBE (token)        server validates token + source IP
server → SUBSCRIBE_ACK            includes current stream timestamp
server → METADATA (initial)
server → data … data … data …
```

Both handshake datagrams are single packets on an unreliable transport, so
the client **retransmits** `SUBSCRIBE` (`RIG_STREAM_NET_SUBSCRIBE_ATTEMPTS`
= 3) rather than failing the open when one is dropped, dividing the caller's
timeout across the attempts. A repeat is safe by design: the server treats a
second `SUBSCRIBE` as a re-subscribe, refreshing the client address and
answering with a fresh `SUBSCRIBE_ACK`. While waiting, the client discards
any frame that is not its `SUBSCRIBE_ACK`, so a `PONG` or an early data
packet cannot be mistaken for a failed handshake.

Keepalive: the client sends `PING` periodically (default interval
`RIG_STREAM_NET_KEEPALIVE_INTERVAL` = 5 s); the server replies `PONG`
and resets the inactivity timer. A unicast stream auto-closes after
`subscribe_timeout` seconds without activity
(`RIGCTLD_SUBSCRIBE_TIMEOUT_DEFAULT` = 30 s). `PING` before `SUBSCRIBE`
is allowed — the server replies `PONG` and keeps waiting. TX streams use
implicit keepalive: any received data packet resets the timer.

Keepalives are not retransmitted; they are periodic, so the ratio of the
timeout to the ping interval **is** the number of consecutive lost pings a
stream survives (6 at the defaults). A link that loses more than that in a
row needs a wider window rather than a faster ping — raise the timeout with
the `stream_keepalive_timeout` rig conf token, rigctld's
`--stream-keepalive-timeout`, or `keepalive_timeout=` on `\stream_open`
(6.1). The client's cadence is set by the `stream_keepalive_interval`
token. Keep the interval well below the timeout: a ratio near 1 lets a
single lost ping end a healthy stream.

### 6.4 Multicast

RX streams may publish to a multicast group via `multicast=ADDR:PORT`
(IPv6 as `[ADDR]:PORT`). The address must be in multicast range (IPv4
224.0.0.0/4, IPv6 ff00::/8). Multicast streams are **RX-only**, skip the
subscribe handshake and all keepalive/timeout logic, and are exempt from
token/IP validation (there is no single client to validate). TTL/hop
limit defaults to the registry value and is overridable per stream;
default 1 (subnet-local). A given group:port may be used by only one
stream.

### 6.5 Security

The protocol is designed for trusted and reliable LAN and private network use.
For public networks/Internet, it is expected that a higher-level application
will either wrap the data in a higher-level protocol, or tunnel it over a VPN-like connection.

The protocol relies on the following security features:

- **Subscribe token** — a random 32-bit value generated at
  `\stream_open`, returned over the TCP channel, and required in every UDP
  packet's header. Mismatched tokens are silently dropped.
- **Source-IP validation** — the server records the TCP client's IP at
  open time and rejects UDP packets from a different source.
- **Client ownership** — each stream records the `client_id` of the TCP
  connection that created it. Operations on an existing stream
  (`close`, `status`, `pause`, `resume`, `mute`, `unmute`,
  `metadata_read`, `drain`) verify the caller owns the stream and return
  `-RIG_EACCESS` otherwise. `\stream_list` hides the UDP port of streams
  the caller does not own. On client disconnect, all of that client's
  streams are closed.

### 6.6 Feeder threads

rigctld runs one feeder thread per active stream:

- **RX feeder**: waits for subscribe, sends initial
  metadata, then loops — read backend ring buffer, packetize, send UDP —
  while polling metadata on its interval and answering PING.
  It reads via the enriched API and stamps data packets with the
  watchdog-checked capture time; on idle it emits time-only packets so
  time keeps advancing.
- **TX feeder**: receives UDP, validates the header,
  detects `seq` gaps, writes samples to the backend ring buffer, and
  dispatches TX metadata frames. It extracts embedded burst
  targets (SOB/EOB) into the backend's target channel.

Per-stream atomic counters (`packet_count`, `gap_count`, `send_drops`)
are readable via `\stream_status`.

### 6.7 Client side

The netrigctl backend implements the client side of the protocol, so
that the client can open streams on a remote rigctld and send/receive
audio/I/Q data over UDP using the same API as local rigctl.

### 6.8 Throughput and socket-buffer sizing

Stream bandwidth is `sample_rate × frame_bytes`, and the packet rate is
that divided by the frame-aligned payload (~1400 bytes). Representative
figures:

| Stream | rate | frame | bandwidth | packets/s |
|--------|-----:|------:|----------:|----------:|
| Audio PCM_S16 mono | 48 kHz | 2 B | 96 kB/s | ~70 |
| I/Q CF32 mono | 192 kHz | 8 B | 1.5 MB/s | ~1100 |
| I/Q CF32 mono | 96 kHz | 8 B | 768 kB/s | ~550 |
| I/Q CF32 dual-coherent | 96 kHz | 16 B | 3.0 MB/s | ~2200 |

At these rates a few milliseconds of scheduling latency can overrun the
kernel's default UDP socket buffer (often well under 256 KB) before the
application-side ring buffer ever helps. Both ends therefore size their
socket buffers from the negotiated rate: `SO_RCVBUF` on the receiving side
(client for RX, server for TX) and `SO_SNDBUF` on the sending side,
defaulting to **250 ms of stream data** clamped to **[256 KB, 8 MB]**. The
requested and kernel-granted sizes are logged at `VERBOSE` (kernels clamp
to their own maximum, and Linux reports double the requested value).

Defaults and per-stream overrides (precedence: per-stream bytes > conf-token
bytes > per-stream ms > conf-token ms > built-in 250 ms; then rate-derived
and clamped):

- `rig_stream_config` fields `transport_buffer_ms` / `transport_buffer_bytes` (client TX path).
- `\stream_open` keys: `transport_buffer_ms=<ms>`, `transport_buffer_bytes=<n>` (server side).
- rig conf tokens `stream_transport_buffer_ms` / `stream_transport_buffer_bytes` (both ends).
- rigctld CLI: `--stream-transport-buffer-ms=MS` (duration) and
  `--stream-transport-buffer-bytes=N` (explicit size; 0 = derive from rate).

#### Jumbo frames and tunable MTU

A datagram carries a 32-byte header plus a frame-aligned sample payload. By
default the sender sizes that payload for a 1500-byte path MTU
(`1500 − 40 (IPv6) − 8 (UDP) − 32 = 1420`, rounded down to a whole frame).
On a fragmentation-free LAN path a larger MTU cuts the packet rate
proportionally, which matters most for high-rate I/Q.

- **Sender knob.** `rig_stream_config.mtu` (or the `mtu=<bytes>` `\stream_open`
  key) sets the path MTU. `0` selects the 1500 default. Any other value is
  clamped to `[576, RIG_STREAM_MAX_DATAGRAM]` (9216, a jumbo-frame ceiling).
- **Receiver liberality.** Every receiver sizes its receive buffer to
  `RIG_STREAM_MAX_DATAGRAM`, so a larger-MTU sender interoperates with an
  existing receiver **without any wire-version change** — this is the only
  MTU behavior with a compatibility deadline, and it is unconditional.
- **Clamp and report-back.** Because the library clamps the request, the
  *effective* payload must be discoverable. `rig_stream_get_max_payload()`
  returns the effective, frame-aligned budget, and rigctld reports it as
  `max_payload: <n>` in the `\stream_open` response. A netrigctl client adopts
  that value so its own TX packetization matches the negotiated path (MTU is
  per-direction: the server packetizes RX, the client packetizes TX).
- **Caveat.** Jumbo requires an end-to-end path that does not fragment; it is
  an off-by-default power-user knob for controlled LANs.

By default the client auto-sizes from the negotiated rate (built-in 250 ms);
the `rig_stream_config.transport_buffer_ms` / `transport_buffer_bytes` fields and the
`stream_transport_buffer_ms` / `stream_transport_buffer_bytes` conf tokens override it.

**OS ceilings.** The kernel silently clamps `SO_RCVBUF`/`SO_SNDBUF` to a
system maximum. To use large buffers for wideband I/Q, raise:
`net.core.rmem_max` / `net.core.wmem_max` (Linux `sysctl`),
`kern.ipc.maxsockbuf` (macOS/BSD), or the registry `SO_MAX_MSG_SIZE`
equivalents (Windows). The VERBOSE log line shows whether the request was
honored.

---

## 7. Stream metadata

Stream metadata frames carry time-aligned rig state (frequencies, VFO,
PTT) correlated to sample position. They reuse the 32-byte header with the
`METADATA` control bit and a 22-byte payload.

The *frozen* semantics of the frequency fields and the metadata model — the
`center_freq`/`vfo_freq` reference plane (dial RF), I/Q spectral sense, sample
full-scale, coherent-channel rules, and the append-only `field_mask` growth —
are normative in §6.2.1 ("v1 semantic contracts").

```c
struct rig_stream_metadata {
    uint32_t field_mask;   /* RIG_STREAM_META_VFO_ID|_PTT|_CENTER_FREQ|_VFO_FREQ */
    uint64_t sample_index; /* sample position (in-memory convenience only) */
    uint8_t  vfo_id;
    uint8_t  ptt;          /* 0 = RX, 1 = TX */
    freq_t   center_freq;  /* RF center of the I/Q window (Hz) */
    freq_t   vfo_freq;     /* primary demod / dial frequency (Hz) */
};
```

Two frequencies, because an I/Q window and the operator's dial are not the
same thing:

- **`center_freq`** is the RF frequency that maps to DC/baseband — it
  places the I/Q window on the band (samples span `center ± rate/2`).
  Mandatory for I/Q streams (`RIG_STREAM_META_CENTER_FREQ`); the frontend
  reports the VFO frequency when the backend supplies no explicit center,
  and panadapter-driven backends (e.g. FlexRadio DAX-IQ) set the true center.
  Unused for audio.
- **`vfo_freq`** is the primary demod (dial) frequency. Optional
  (`RIG_STREAM_META_VFO_FREQ`): for a multi-slice I/Q window it is the
  primary slice or absent. For audio it is the only frequency.

Both are absolute `freq_t` doubles (matching Hamlib's native frequency
type and Linrad's `cfreq`), carried as 8-byte big-endian IEEE-754.

Wire payload (22 bytes, big-endian; the `timestamp` lives in the packet
header, not the payload):

```
Offset Size Field         Mask bit
  0     4   field_mask
  4     1   vfo_id         RIG_STREAM_META_VFO_ID       (1<<0)
  5     1   ptt            RIG_STREAM_META_PTT       (1<<1)
  6     8   center_freq    RIG_STREAM_META_CENTER_FREQ (1<<2)
 14     8   vfo_freq       RIG_STREAM_META_VFO_FREQ  (1<<3)
```

`field_mask` marks which fields are meaningful; the mask-bit order follows
the field/offset order. Fields have **fixed offsets** — an optional field
still occupies its slot, and its mask bit gates its *meaning*, never its
byte-presence. The wire format is forward-compatible: new fields append
after offset 22 with new mask bits, and receivers use the header
`payload_len` to size the payload (skipping unknown trailing bytes).
`payload_len` covers trailing growth, not interior omission.

**Coherent channels vs. independent windows** are two orthogonal axes and
must not be conflated. Phase-coherent channels sharing an LO/clock (X/Y
polarization diversity for EME) travel as **one stream with N
per-sample-interleaved channels and one `center_freq`**. Independent
spectral windows (such as FlexRadio panadapters/slices) travel as **separate
streams, each with its own `stream_id`, `center_freq`, and `vfo_freq`** —
they are not coherent, and cross-stream alignment is only as good as both
streams' time anchors.

**Source of truth is `rig_cache`.** A working CAT backend already keeps
the per-VFO cache current, so basic metadata needs no backend work — the
frontend reads the cache via `rig_stream_read_metadata()`. RX
metadata is sent two ways, complementary: a **change-detected poll** at
`metadata_interval` (wall-clock, clamped 25–1000 ms, default 100 ms) —
the ceiling on how long a *change* waits to reach the client — and an
**unconditional refresh** at `metadata_refresh`, whose cadence is measured
in **stream-data duration** (frames sent × sample period, default 100 ms) —
a floor on send frequency so a lost metadata frame heals without waiting
for the next change. `metadata_refresh = 0` emits a metadata frame with
every data packet (closest to per-packet metadata). Both are configurable
per stream (`\stream_open` keys), as rig conf tokens
(`stream_metadata_interval` / `stream_metadata_refresh`), and as rigctld
CLI defaults (`--stream-metadata-interval` / `--stream-metadata-refresh`);
precedence is per-stream key > conf token > CLI default. TX metadata
flows the other way: a client writes it, and the frontend either calls
the backend's `stream_apply_metadata` (for radios that support timed
frequency/PTT changes) or falls back to `rig_set_freq()` etc.

Mode, filter width, and S-meter are deliberately excluded, since they are
queryable via CAT and it is not critical to track them at sample-accurate level.

---

## 8. Time model: capture time, gaps, and timed transmit

The time model gives each sample an absolute UTC capture time, reports
losses as exact sample-indexed discontinuities, and lets transmit bursts
be scheduled to a UTC instant. It rides alongside the byte ring buffer
through sparse time anchors and an enriched read, so the data path is
unchanged. The subsections below cover representation, anchors and the
read path, the staleness watchdog, the on-wire TIME block, gap handling,
and timed transmit.

### 8.1 What the ring buffer preserves

The ring buffer is a byte stream: it strips packet boundaries and
radio-side timestamps. What survives by itself: a monotonic `seq` (per
packet, 32-bit) for app-link gap detection, and a 64-bit `timestamp`
sample counter that starts at 0 and effectively never wraps — a relative
sample index, **not** an absolute capture time. The time model restores
absolute time without changing the data path: sparse **time anchors**
correlate the sample counter to UTC, the same pattern as GNU Radio
`rx_time` tags, RTCP sender reports, and VITA-49/UHD per-packet time.

### 8.2 Representation and timescale

Absolute time is `int64 seconds` (Unix epoch, **UTC**) + `uint64
picoseconds` (0…999,999,999,999) — the VITA-49 "real-time" unit: decimal,
1 ps resolution, finer than any real source (GPS/PTP ~ns; host µs–ms).
A backend with an atomic source (GPS/TAI) converts to UTC using the leap
offset that source provides; the `source` byte conveys provenance only,
never a different timescale.

Leap seconds: a UTC wall-clock *interval* straddling a leap is off by
~1 s. In practice interval math uses the leap-immune **sample counter**
(`sample_index` + `sample_rate`); the UTC anchor only labels absolute
instants. A flag bit is reserved for anchors that knowingly straddle a
leap boundary. No TAI/leap-offset field is carried.

### 8.3 Sources, accuracy, flags

```c
enum rig_stream_time_source {
    RIG_STREAM_TIME_SRC_NONE = 0,   /* no reference */
    RIG_STREAM_TIME_SRC_HOST,       /* host CLOCK_REALTIME */
    RIG_STREAM_TIME_SRC_GPS,        /* GPS-disciplined */
    RIG_STREAM_TIME_SRC_NTP,        /* NTP-disciplined host clock */
    RIG_STREAM_TIME_SRC_PTP,        /* IEEE 1588 */
    RIG_STREAM_TIME_SRC_RADIO       /* radio-reported, untraceable epoch */
};

enum rig_stream_time_accuracy {     /* coarse absolute-UTC quality hint */
    RIG_STREAM_TIME_ACC_UNKNOWN = 0,
    RIG_STREAM_TIME_ACC_COARSE,         /* > 1 ms */
    RIG_STREAM_TIME_ACC_MS,             /* ~1 ms (NTP-synced host) */
    RIG_STREAM_TIME_ACC_US,             /* ~1 us (microsecond) */
    RIG_STREAM_TIME_ACC_100NS           /* <= 100 ns (GPS/PPS) */
};

/* flags byte — RX uses LOCKED/HOLDOVER/DISCONTINUITY/SAMPLE_REFERENCED,
 * TX uses SOB/EOB */
#define RIG_STREAM_TIME_FLAG_LOCKED            (1<<0)
#define RIG_STREAM_TIME_FLAG_HOLDOVER          (1<<1)  /* lost lock, coasting */
#define RIG_STREAM_TIME_FLAG_DISCONTINUITY     (1<<2)  /* gap/overrun precedes */
#define RIG_STREAM_TIME_FLAG_SOB               (1<<3)  /* TX: start of burst */
#define RIG_STREAM_TIME_FLAG_EOB               (1<<4)  /* TX: end of burst */
#define RIG_STREAM_TIME_FLAG_TX_TIMED          (1<<0)  /* TX: seconds/picoseconds
                                                     hold a scheduled UTC
                                                     instant (shares the bit
                                                     with RX-only LOCKED) */
#define RIG_STREAM_TIME_FLAG_SAMPLE_REFERENCED (1<<5)  /* tied to the radio
                                                     sample clock */
/* bit 6 reserved: leap-second straddle */
#define RIG_STREAM_TIME_FLAG_DISC_OVERRUN      (1<<7)  /* with DISCONTINUITY:
                                                     cause was a consumer-
                                                     side ring overrun, not
                                                     radio/network loss */
```

For applications that need time-of-arrival (TOA) accuracy,
`SAMPLE_REFERENCED` means the time is tied to the radio's sample/ADC clock.
Without it, the anchor is a host-arrival estimate carrying tens of ms of
jittery pipeline latency. An application should treat data as TDoA/TOA-grade
only when `SAMPLE_REFERENCED` is set **and** accuracy is `US` or better.
`source=HOST` anchors are deliberately not latency-corrected:
they answer "which UTC second," not "which microsecond."

### 8.4 Anchors and the enriched read

```c
struct rig_stream_time_anchor {     /* sample_index ↔ wall-clock */
    uint64_t sample_index;          /* ring-buffer producer domain */
    int64_t  seconds;
    uint64_t picoseconds;
    uint8_t  source, flags, accuracy;
};

struct rig_stream_read_info {       /* filled by rig_stream_read */
    uint64_t sample_index;          /* producer index of first byte returned */
    uint32_t dropped_samples;       /* known-size hole before this read,
                                       any cause (gap and/or overrun) */
    uint8_t  drop_flags;            /* RIG_STREAM_DROP_* cause attribution */
    int      time_valid;
    int64_t  seconds;
    uint64_t picoseconds;
    uint8_t  time_source, time_flags, time_accuracy;
};

/* drop_flags bits */
#define RIG_STREAM_DROP_GAP      (1<<0)  /* radio/network-side marked gap */
#define RIG_STREAM_DROP_OVERRUN  (1<<1)  /* ring overrun (consumer slow) */
#define RIG_STREAM_DROP_UNSIZED  (1<<2)  /* an unknown-size gap also precedes;
                                            dropped_samples is a lower bound */
#define RIG_STREAM_DROP_LINK     (1<<3)  /* network client: app-link UDP loss */
```

The backend pushes anchors (`rig_stream_push_time_anchor()`) at
stream start, **at least every 1 s**, and — flagged DISCONTINUITY —
right after any detected gap; per-packet where it has per-packet hardware
time. A small per-stream anchor ring (depth 16, drop-oldest) sits beside
the byte ring buffer, which is unchanged. `rig_stream_read(…, info)`
reports the producer sample index of the first returned byte (computed
from a monotonic ring-buffer `write_total`, so it stays correct across
overruns), the drop count since the previous read, and the wall-clock of
that sample interpolated from the newest anchor at or before it
(`Δps = Δsamples·10¹²/rate`, 128-bit intermediate, carry into seconds).

### 8.5 Staleness watchdog

Interpolation assumes the nominal sample rate, so time quality decays
with anchor age (~180 ms/hour at 50 ppm). The read path computes anchor
staleness for free and keeps the reported grade honest: past
`time_stale_coarse_ms` it downgrades `time_accuracy` to `COARSE`; past
`time_stale_invalidate_ms` it clears `time_valid`. It never fabricates a
fresh anchor. Thresholds (defaults **1000 / 5000 ms**, `coarse ≤
invalidate` validated at open) are configurable per stream
(`rig_stream_config`; `\stream_open` keys) and as defaults
for all streams (conf tokens `stream_time_stale_coarse` /
`stream_time_stale_invalidate`; rigctld CLI `--stream-time-stale-coarse`
/ `--stream-time-stale-invalidate`).

### 8.6 On the wire: the TIME block

Control bit `0x0020` (TIME) marks a packet whose payload **begins with a
20-byte time block** (big-endian); any remaining `payload_len − 20` bytes
are samples. `header.timestamp` is the sample index the time applies to.

```
Offset Size Field
  0     8   seconds      (int64, UTC)
  8     8   picoseconds  (uint64, 0..999,999,999,999)
 16     1   source
 17     1   flags
 18     1   accuracy
 19     1   reserved (must be zero)
```

Three cases, one mechanism: **time + data** (normal), **data only** (TIME
bit clear — the client holds the last block and interpolates), **time
only** (`payload_len == 20`, keeps time advancing while idle/paused).
On RX the block is capture time from the watchdog-checked read path; on
TX it is the burst target with SOB/EOB flags, and `source`/`accuracy`
are sent as 0 and ignored. The TIME bit is valid only on data and
time-only packets — never combined with METADATA, WRITE_STATUS, ERROR,
SUBSCRIBE, SUBSCRIBE_ACK, PING, or PONG (two payload-prefix definitions would
be ambiguous; violating packets are dropped and logged). The netrigctl
client pushes received blocks into its local anchor ring, so a remote
consumer's enriched read behaves identically to direct mode.

**Three-way loss classification.** `header.timestamp` carries the
producer sample index, so any upstream hole appears as a timestamp
jump with `seq` intact. A data packet whose timestamp jumps MUST carry a
TIME block with DISCONTINUITY, and `DISC_OVERRUN` distinguishes the cause.
The client classifies and replays each hole into its local index domain,
so its stats and `drop_flags` match direct mode:

| Observation | Cause | Stats bucket |
|---|---|---|
| `seq` gap (size from timestamp delta) | app-link UDP loss | `link_loss` / `dropped_samples_link` |
| timestamp jump, `seq` intact, `DISC_OVERRUN` clear | radio/network gap upstream | `gaps` / `dropped_samples_gap` |
| timestamp jump, `seq` intact, `DISC_OVERRUN` set | server ring overrun | `overruns` / `dropped_samples_overrun` |
| DISCONTINUITY block, no jump | unsized upstream gap | `gaps` + `gaps_unknown` |

Fallback: a timestamp jump arriving without a TIME block (the stamped
packet itself was lost) classifies as a radio/network gap.

### 8.7 Gaps and discontinuities

Gaps enter at four points: radio→backend loss, backend processing loss,
ring-buffer overrun, feeder→app UDP loss. The last is covered by the
protocol `seq` (classified at the client). For the rest, the
**virtual index skip** carries the loss to the application: the backend
calls `rig_stream_mark_gap(stream, n)` (n = exact missing sample count,
0 = unknown), which advances the producer sample-index domain without
writing bytes — mechanically identical to what an overrun already does,
just announced by the backend. Consequences:

- The app receives **all** losses through one interface:
  `read_info.dropped_samples` (exact size) + DISCONTINUITY (position),
  cause-attributed by `drop_flags` and the per-cause stats totals.
- The sample-index domain matches radio emission, so capture-time
  interpolation stays correct **across** the gap, even against an older
  anchor.
- On the wire, `header.timestamp` jumps by the hole — no format change;
  remote clients reconstruct it mechanically.
- Fill policy belongs to the app, which now has exact position + size: it
  can insert zeros, interpolate, or reset its DSP.

Per family: **audio RX** keeps zero-fill for smooth live playback (the
zeros are real samples — `dropped_samples` stays 0) plus the
DISCONTINUITY anchor so recorders know they are synthetic. **I/Q RX**
uses `mark_gap` + anchor, no fill — mis-sized fills are the thing that
corrupts FFT phase, and exactly-sized fills are reproducible app-side.
Unknown sizes degrade gracefully: `mark_gap(0)` → DROP_UNSIZED,
`gaps_unknown`, position-only.

### 8.8 Timed transmit

```c
struct rig_stream_write_info {
    int      time_valid;            /* 0 = send immediately */
    int64_t  seconds;               /* UTC target */
    uint64_t picoseconds;
    uint8_t  flags;                 /* SOB/EOB */
};
```

`rig_stream_write(…, info)` with `time_valid` schedules a burst: samples
go to the ring buffer, the target to a mirror per-stream target ring that
the backend drains with `rig_stream_pop_tx_target()`. Two tiers,
declared via `caps_flags`:

- **Tier 1 — coarse** (`CAP_TIMED_TX_COARSE`): the backend waits for the
  target instant (device clock if `CAP_HW_TIME`, else host
  `CLOCK_REALTIME`), then starts continuous play-out. ~ms accuracy —
  right for FT8/WSPR/beacons/EME windows.
- **Tier 2 — sample-accurate** (`CAP_TIMED_TX_SAMPLE`): the target is
  handed to hardware that schedules emission (USRP-class). No current
  backend has such hardware; the framework supports it and the dummy
  backend simulates it.

PTT: if the TX caps entry sets `CAP_BURST_PTT`, SOB keys PTT at the
target and EOB unkeys ; otherwise the app drives PTT itself.
Targets beyond `tx_schedule_horizon_ms` are
rejected at write time with `-RIG_EINVAL`; past targets transmit
immediately and increment the `tx_late` counter. Apps schedule in
UTC; `rig_stream_get_hardware_time()` returns the radio clock as an
anchor (source/flags/accuracy) so an app knows the achievable precision
before scheduling.

The `ERROR` control bit (0x0010) is **reserved**: no ERROR frame is emitted
yet, and its payload format is undefined. The netrigctl client drops any
received ERROR frame defensively, so an error payload is never mistaken for
sample data. Reporting a missed timed-TX slot (or a TX ring under/overrun)
to a remote client is handled by the `WRITE_STATUS` frame (§8.9), not ERROR.

### 8.9 On the wire: the write-status block

Control bit `0x0080` (WRITE_STATUS) marks a **server → client** packet whose
payload is a **36-byte write-status block** (big-endian, no sample data). It
reports an async TX problem — a late timed burst, or a TX ring under/overrun —
back to a TX client, which cannot otherwise observe events that happen on the
server's transmit side. The server's TX feeder drains the backend stream's
write-status event FIFO and forwards each event with its full detail; the
producer-side `sample_index` rides the packet header `timestamp`.

```
Offset Size Field
  0     2   event           (uint16: 1=LATE, 2=UNDERRUN, 3=OVERRUN)
  2     2   flags           (bit 0 = TIME_VALID, bit 1 = REMOTE; rest reserved)
  4     4   dropped_samples (uint32; 0 = N/A)
  8     8   lateness        (int64, samples; 0 = N/A)
 16     8   seconds         (int64, UTC; valid if TIME_VALID)
 24     8   picoseconds     (uint64, 0..999,999,999,999)
 32     1   time_source
 33     1   time_flags
 34     1   time_accuracy
 35     1   reserved (0)
```

The frame draws a `seq` value, so a client accounts for it before inferring
app-link loss (like METADATA and time-only frames). On receipt the netrigctl
client marks the event **REMOTE** and records it (bumping `remote_overruns` /
`remote_underruns`, or `tx_late` for a late burst).

**Delivery is best-effort.** Each event is sent once and is never
retransmitted or acknowledged, so a dropped datagram loses that event's
detail permanently. Because the frame consumes a `seq` value, the loss is
still *visible*: it lands in the receiver's normal gap accounting, so a
client sees that something went missing even though it cannot recover what.
Treat write-status events as diagnostics rather than as a channel that must
not lose a message — a TX application that has to be certain a burst was
sent should confirm it from its own `stats` and timing, not from the arrival
of an event.

The application drains events through one blocking-with-timeout call
(`timeout_ms < 0` blocks, `0` polls, `> 0` bounds):

```c
struct rig_stream_write_status st;
while (rig_stream_wait_write_status(rig, stream, &st, 0) == RIG_OK) {
    /* st.event, st.sample_index, st.dropped_samples, st.lateness,
       st.seconds/picoseconds (if st.time_valid), st.flags & …_REMOTE */
}
```

Events queue in a bounded per-stream FIFO (drop-oldest on overflow, counted by
`stats.write_events_dropped`). **This is TX-stream only** — RX losses arrive inline
via `rig_stream_read_info` instead. In direct (non-network) mode the backend
records events locally, so the same API works without a server.

---

## 9. Format conversion

(Codec-frame streams are exempt from everything in this section: no
conversion stage ever applies to compressed frames — they pass through
untouched, §3.2/§4 — and a codec request must match the native
declaration exactly.)

Per-client format, rate and channel conversion is a **frontend**
concern (the native/effective capability split, §3.3): `rig_stream_open()`
installs a persistent, stateful pipeline (an internal `struct stream_conv`
in `src/stream_convert.c` — channel map → float pivot → stateful
libsamplerate resampler → destination format) on the producer side of the
ring whenever the request is not native. The pipeline's resampler quality
is selected by the rig-level conf token `stream_resample_quality` —
`best`, `medium` (default) or `fast`, mapping to libsamplerate's
corresponding sinc converters — read when a pipeline is created, so set
it before opening the stream (server-side for network clients, e.g.
`rigctld --set-conf=stream_resample_quality=best`). Backends produce and consume their native format only
(§5). The helpers below are a standalone library — the codec layer (§9.1)
chains them, and a backend with a genuinely special path may still call
them directly:

`src/stream_convert.{c,h}` provides pure-C conversion (no required
dependencies):

```c
int    rig_stream_format_sample_size(rig_stream_format_t format);
int    rig_stream_convert(const void *src, rig_stream_format_t src_format,
                          void *dst, rig_stream_format_t dst_format,
                          size_t sample_count, int channels);
int    rig_stream_convert_channels(const void *src, int src_channels,
                                   void *dst, int dst_channels,
                                   size_t sample_count, rig_stream_format_t format);
int    rig_stream_resample(const float *src, int src_rate,
                           float *dst, int dst_rate,
                           size_t src_samples, size_t *dst_samples,
                           int channels, int quality);
```

- `rig_stream_convert()` converts within a family (audio↔audio or
  I/Q↔I/Q); it cannot cross between audio and I/Q.
- `rig_stream_convert_channels()` is format-aware: mono→stereo duplicates,
  stereo→mono averages with widened arithmetic per format.
- `rig_stream_resample()` is F32-only and uses libsamplerate (enabled by
  default; build `--without-samplerate` to omit it, then it returns −1).
  Quality is `RIG_RESAMPLE_BEST|MEDIUM|FAST`.
- Opus is a format flag only — no codec is wired yet. Core streaming works
  with plain PCM.

Recommended order inside a backend thread:
**channels → format → resample**.

### 9.1 Device audio codecs

Some radios carry audio over the device link in a companded or compressed
codec (μ-law, A-law, device-specific ADPCM) rather than raw PCM. These are
*not* stream formats (§3.2) — they are never advertised in `stream_caps`
and never reach the application. `src/stream_codec.{c,h}` terminates them,
keyed on its own internal `rig_audio_codec_t` enum:

```c
struct rig_audio_codec_state *
       rig_audio_codec_open(rig_audio_codec_t codec, int channels);
void   rig_audio_codec_reset(struct rig_audio_codec_state *st);
void   rig_audio_codec_close(struct rig_audio_codec_state *st);

int    rig_audio_convert_to_pcm(struct rig_audio_codec_state *st,
                                const void *src, size_t src_bytes,
                                rig_stream_format_t pcm_format,
                                void *pcm, size_t pcm_cap, size_t *pcm_bytes);
int    rig_audio_convert_from_pcm(struct rig_audio_codec_state *st,
                                  rig_stream_format_t pcm_format,
                                  const void *pcm, size_t pcm_bytes,
                                  void *dst, size_t dst_cap, size_t *dst_bytes);
```

The layer is separate from `rig_stream_convert()` because device codecs
need **per-stream state** (ADPCM's predictor, reset at a gap) and have
**non-1:1 byte ratios** (nibble-packed ADPCM), neither of which the
stateless per-sample converter expresses. Every codec pivots on 16-bit
linear internally; the two calls take a caller-chosen PCM `pcm_format` and
chain `rig_stream_convert()` for the final hop, so a backend gets the
format it wants in one call and no second conversion matrix is needed.
`RIG_AUDIO_CODEC_NONE` is a validated passthrough (device link already
PCM), so a backend wires one pipeline whether or not a codec is in play.

μ-law and A-law (ITU-T G.711, stateless) and IMA/DVI 4-bit ADPCM
(`RIG_AUDIO_CODEC_ADPCM_IMA`, single channel) are implemented in both
directions. ADPCM carries a running step index between blocks on the
encode side, which `rig_audio_codec_reset()` clears; decoding needs no
carried state because each block begins with its own predictor and index.
No backend selects a codec yet, so the layer is exercised only by its unit
tests. The RX/TX pipeline
that chains these calls with `rig_stream_convert()` is in
`HAMLIB_STREAMING_BACKEND_GUIDE.md` §9.

---

## 10. Protocol rationale

The transport is a **custom 32-byte binary header over UDP**. It targets
a LAN, real-time use case where the freshest data matters most: the
reliability model is 32-bit sequence numbers for gap detection with **no
retransmit** — on loss, the application inserts silence (audio) or zeros
(I/Q). Adopting RTP or VITA-49 was considered; the custom header was
chosen for requirement fit. The comparisons below are about that fit, not
about shortcomings of those standards.

**Relative to RTP (RFC 3550).** RTP is optimized for codec-based media
with a payload type negotiated once per session over SDP. Our streams
carry raw I/Q and float32 audio, for which there is no registered RTP
payload type, and the sample rate changes per packet as the operator
changes VFO or bandwidth — which RTP models as a session-level constant.
Meeting our requirements in RTP would mean dynamic payload types plus
per-change SDP, and the SDR tools we interoperate with (GNU Radio,
SoapySDR) do not use RTP, so its ecosystem tooling would not apply here.
The header is nonetheless kept structurally RTP-like (version, sequence,
timestamp, stream id), so an RTP-for-audio compatibility mode could be
added later if standard-tool interop becomes a requirement.

**Relative to VITA-49 (VRT).** VITA-49 is a rich, self-describing format
designed for instrumentation and SIGINT links. Several of its design
points suit that domain more than ours: a 4-bit packet count, 32-bit-word
framing, and out-of-band context packets. At audio and I/Q packet rates
we want a wider sequence space for gap detection than 4 bits provides, and
we prefer every packet to be independently decodable without caching a
separate context packet — a fixed binary header meets both needs
directly. (Ettus/USRP, which builds on VITA-49, uses a compact fixed
8-byte CHDR header on its own transport for similar reasons.)

**The custom header** puts all metadata in every packet (stateless and
self-describing), uses a 32-bit sequence (wraps in ~24 days even at
2 MHz I/Q) and a 64-bit sample-count timestamp (effectively never wraps),
and is about 200 lines to implement, with a straightforward Wireshark
dissector if one is wanted.

The same priorities shaped two time-model choices. Absolute time
rides **in** the data packets (a flagged 20-byte payload prefix, as
VITA-49, UHD, and SoapySDR also do) rather than in a separate RTCP-style
time frame: at our payload sizes the block is ~1.4% overhead, every
packet stays self-describing (a lossy or late-joining client syncs from
the first stamped packet it sees), and an out-of-band time frame would
require every wire client to run a correlation state machine — a
trade-off that pays off mainly for small RTP voice packets, not our larger
frames. Time is expressed as **UTC seconds + picoseconds** (the VITA-49
real-time unit: decimal, sub-nanosecond, finer than any source we expect)
rather than NTP's binary 2⁻³²/2⁻⁶⁴ fractions, favoring a representation
that is easy to read and compute with.

---

## 11. Reference backends

### Dummy (`rigs/dummy/dummy_stream.c`)

The test/reference backend. Declares all four stream types with PCM and
I/Q formats; tone / silence / loopback / counter generator modes;
per-stream generator threads paced by `nanosleep`; full format/channel/
rate conversion in the loopback path, including multi-channel I/Q (each
channel lane is carried through independently). Configurable via `set_conf`
(`stream_mode`, `stream_tone_freq`, …). It is the worked example the
backend guide refers to. It pushes host-clock anchors, offers a
synthetic-gap conf token (`stream_synth_gap`) for tests, and runs a TX
scheduler that simulates both timed-transmit tiers with burst PTT.

---

## 12. Trying it out

The dummy backend (model 1) implements all four stream types in software, so
the whole path — ring buffer, conversion, metadata, time model — can be
exercised without a radio. `rigstreamtest` (`tests/rigstreamtest.c`) is the
tool that drives it. It is a diagnostic tool rather than a unit test, so a
plain `make` builds it; it is not installed, and runs from the build tree.

Run a stream against the dummy:

```sh
./tests/rigstreamtest -m 1 -t audio_rx -d 5           # RX audio
./tests/rigstreamtest -m 1 -t iq_rx -s 48000 -d 5     # RX I/Q at 48 kHz
./tests/rigstreamtest -m 1 -t loopback -d 5           # TX -> ring -> RX
```

`rigctl -m <model> --dump-caps` prints a `Data streaming capabilities:`
block — one entry per line in the **same key=value grammar as
`\stream_caps`** (§6.1), in declaration form (no `native_*` keys: a
model declaration has no derived view), plus a
`Has data streaming support:` summary that is Y only when the caps and
the required `stream_open`/`stream_close` hooks are all present. It also
runs sanity checks over the declarations — inconsistent caps/hook
pairings and malformed rate/channel lists are reported as backend
warnings. On a netrigctl rig the dump shows the served session view,
byte-identical to the `\stream_caps` lines.

To exercise the network path, run rigctld with the dummy backend and point the
tool at it through netrigctl (model 2):

```sh
./tests/rigctld -m 1 -t 5555 &
./tests/rigstreamtest -m 2 -r localhost:5555 -t audio_rx -d 5
```

**Reading the output.** A progress line is printed each second:

```
[   2s] bytes=245760  gaps=0(0 unsized)  overruns=0  underruns=0  link=0  dropped(gap/ovr/link)=0/0/0
```

- `bytes` should climb at roughly `sample_rate x channels x bytes_per_sample`.
- `gaps` — radio- or link-side losses reported by the backend; the
  parenthesised count is the subset whose size was unknown.
- `overruns` — the ring filled because the application did not read in time;
  `underruns` — a read found no data before its timeout.
- `link` — datagrams lost on the app link, network path only.
- `dropped(gap/ovr/link)` — per-cause sample totals behind those events.

A healthy dummy run is zeros across the board. The closing `Done (result=N)`
line carries the exit code, and its meaning differs by mode: a single-shot run
reports open/IO failures only, so judge stream health from the counters
yourself; the soak modes (`--rx-secs`/`--tx-secs`, `--full-duplex`) additionally
fold every tallied issue into it, so `result=0` there means a genuinely clean
run. `--help` lists the remaining options.

`-P`/`--ptt` and `--power` key a real transmitter. They are meant for hardware
runs and should be pointed into a dummy load.

---

## 13. Building and tests

libsamplerate is the subsystem's only optional dependency, used by
`rig_stream_resample()`. `configure` reports it as `With libsamplerate support`;
building `--without-samplerate` omits it, after which resampling returns -1.
Everything else in the subsystem is built unconditionally.

The unit and integration tests live in `test/` and use
[acutest](https://github.com/mity/acutest), a single-header framework vendored
as `test/acutest.h`, so they add no build dependency:

```sh
make -C test check               # build and run the whole suite
./test/test_stream_api           # run one binary directly
./test/test_stream_api --list    # list its cases
./test/test_stream_api open_close  # run a single case
```

Tests that need a peer start it themselves: the netrigctl and rigctld command
suites fork a real `rigctld`, and the FlexRadio tests drive the `simflex`
simulator. An interrupted run can leave those daemons behind and the next run
then fails to bind its port, so reap strays before re-running.

---

## 14. Status and limitations

**Status:** API is implemented and tested, but remains in development,
so the wire protocol is still subject to change.

**Known limitations:**

- I/Q gap zero-fill is deliberately avoided (mis-sized fills corrupt FFT
  phase) — I/Q gaps surface through `dropped_samples`, leaving
  the fill policy to the app.
- Opus is defined as a stream format but has no codec implementation. The
  device-link codecs (G.711 μ-law/A-law and IMA ADPCM) are implemented in
  both directions, but no backend selects one yet.
- Local rigctl (non-daemon) returns `-RIG_ENAVAIL` for stream commands.
- Tier-1 timed TX is ~ms accurate (host/transport-bound) — right for
  FT8/WSPR-class alignment, not precise pulse timing. Sample-accurate
  Tier 2 awaits a USRP-class backend for hardware validation.
- Cross-stream sample alignment (e.g. dual-RX diversity correlation) is
  only as good as both streams' anchors — stream timestamps are
  per-stream with independent epochs. True phase/sample coherence is a
  hardware property (shared LO/clock), out of scope for this transport.
- Sub-µs absolute capture time requires a backend whose radio stamps
  samples against GPS (`SAMPLE_REFERENCED`).

---

## 15. Source map

| Concern | Files |
|---------|-------|
| Public types & C API | `include/hamlib/rig.h`, `include/hamlib/rig_state.h` (`stream_state`) |
| Stream lifecycle, read/write, metadata | `src/stream.c`, `src/stream.h` |
| Ring buffer | `src/stream_ringbuf.c`, `src/stream_ringbuf.h` |
| Loss accounting + producer index | `src/stream_account.c`, `src/stream_account.h` |
| Capture-time anchors | `src/stream_anchor.c`, `src/stream_anchor.h` |
| Time conversion + interpolation helpers | `src/stream_time.c`, `src/stream_time.h` |
| Format conversion (PCM/I-Q, channel, rate) | `src/stream_convert.c`, `src/stream_convert.h` |
| Audio codecs (G.711 µ-law/A-law, ADPCM) | `src/stream_codec.c`, `src/stream_codec.h` |
| Wire format (pack/unpack, names, indices) | `src/stream_proto.c`, `src/stream_proto.h` |
| Client-side UDP session | `src/stream_net.c`, `src/stream_net.h` |
| rigctld registry & feeders | `tests/rigctld_stream.c`, `tests/rigctld_stream.h` |
| rigctld command handlers | `tests/rigctl_parse.c` (codes 0xb0–0xba) |
| Dummy backend (reference) | `rigs/dummy/dummy_stream.{c,h}` |
| Netrigctl client backend | `rigs/dummy/netrigctl.c` |
