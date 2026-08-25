# Hamlib Stream API — Backend Developer Guide

Step-by-step guide for adding audio and I/Q streaming to an existing
Hamlib rig backend.

---

## 1. Introduction

The Hamlib streaming subsystem lets applications receive and transmit
real-time audio and I/Q data through the standard Hamlib C API. The
architecture follows Hamlib's frontend/backend split:

- **Frontend** (`src/stream.c`) — manages stream handles, ring buffers,
  format validation, overrun/underrun counting. Backends never touch this.
- **Backend** (your code) — implements `stream_open` / `stream_close`
  callbacks and pushes/pulls sample data through ring buffers.

```
RX:  Hardware → backend thread → stream_backend_write() → [conversion] → [ring buffer] → rig_stream_read() → app
TX:  App → rig_stream_write() → [conversion] → [ring buffer] → stream_ringbuf_read() → backend thread → hardware
```

Conversion (format/rate/channels, when the application's request is not
hardware-native) is installed by the frontend on the producer side of the
ring in both directions; the backend always produces and consumes its
native format (Sections 2, 3 and 5).

**Prerequisites:** A working backend with `rig_caps` populated and CAT
control functional. Familiarity with Hamlib's backend pattern.

**Reference implementation:** `rigs/dummy/dummy_stream.c` and
`rigs/dummy/dummy_stream.h`. Read alongside this guide.

**Design rationale and protocol reference:** `HAMLIB_STREAMING.md`


---

## 2. Declare Stream Capabilities

Declare the **hardware-native truth**: the formats, sample rates and
channel counts the radio actually produces or accepts — nothing more. The
frontend derives the wider *effective* set it advertises to applications
(the whole PCM/I/Q format family, standard and integer-divided rates when
libsamplerate is built in, audio mono↔stereo mapping) and installs any
conversion itself at `rig_stream_open()`. Your backend never sees a
converted request — it always runs at a native configuration you declared
(see Section 3, "What the frontend handles for you"; the full derivation
and acceptance rules are in `HAMLIB_STREAMING.md` §3.3). Do **not**
advertise combinations you would have to convert to serve: the frontend
serves those through conversion, and `require_native` relies on your
declaration being hardware truth.

`rig_caps.stream_caps` is a **pointer** to a 0-terminated
`struct rig_stream_caps` array (not an embedded array — so the descriptor
can gain fields without changing `sizeof(rig_caps)`). Point it at a
`static const` array.

### The `rig_stream_caps` structure

Defined in `include/hamlib/rig.h`:

```c
struct rig_stream_caps {
    rig_stream_type_t type;                          /* Stream direction */
    rig_stream_format_t formats;                     /* Bitmask of NATIVE formats (uint32_t) */
    int32_t sample_rates[HAMLIB_MAX_STREAM_RATES];   /* Native rates, 0-terminated */
    int32_t channels[HAMLIB_MAX_STREAM_CHANNEL_COUNTS]; /* Openable channel
                                                      * counts, 0-terminated
                                                      * ascending list
                                                      * (1=mono, 2=stereo) */
    int32_t max_streams;                             /* Concurrent streams of this type */
    int32_t tx_schedule_horizon_ms;                  /* Max timed-TX lead time (0 = not schedulable) */
    uint64_t caps_flags;                             /* RIG_STREAM_CAP_* (timed TX; Section 6.1) */
    /* native_formats / native_sample_rates / native_channels:
     * leave ZERO. The frontend fills them in the derived caps it serves
     * to applications. Setting native_formats yourself declares BOTH
     * views pre-derived (only a relaying backend like netrigctl does
     * this — the frontend then serves your entry verbatim and delegates
     * conversion to your far side). */
    rig_stream_format_t native_formats;
    int32_t native_sample_rates[HAMLIB_MAX_STREAM_RATES];
    int32_t native_channels[HAMLIB_MAX_STREAM_CHANNEL_COUNTS];
    /* ...tail-only ABI headroom (never declare or touch) ... */
};
```

### Stream types

| Type                    | Value | Direction | Description            |
|-------------------------|-------|-----------|------------------------|
| `RIG_STREAM_TYPE_AUDIO_RX`  | 0     | Rig → App | Receive audio          |
| `RIG_STREAM_TYPE_AUDIO_TX`  | 1     | App → Rig | Transmit audio         |
| `RIG_STREAM_TYPE_IQ_RX`     | 2     | Rig → App | Receive I/Q samples    |
| `RIG_STREAM_TYPE_IQ_TX`     | 3     | App → Rig | Transmit I/Q samples   |

### Format flags

**Audio formats** (bits 0-4 assigned; bits 5-15 reserved):

| Flag                            | Bytes/sample | Description             |
|---------------------------------|-------------|-------------------------|
| `RIG_STREAM_FORMAT_PCM_S8`     | 1           | Signed 8-bit            |
| `RIG_STREAM_FORMAT_PCM_U8`     | 1           | Unsigned 8-bit          |
| `RIG_STREAM_FORMAT_PCM_S16`    | 2           | Signed 16-bit           |
| `RIG_STREAM_FORMAT_PCM_F32`    | 4           | IEEE 754 float          |
| `RIG_STREAM_FORMAT_OPUS`       | variable    | Opus codec frames (passthrough; no transcode) |

The wire sample payload is fixed little-endian; there are no per-format
byte-order (LE/BE) variants. ADPCM is a device-link codec
(`rig_audio_codec_t`), **not** a stream format — see Section 9.1.

Codec stream formats (OPUS, and any future bit outside the raw PCM/I-Q
families) are opaque packet streams: the frontend applies no conversion
stage to them, a client request must match a declared rate and channel
count exactly, and a codec-only caps entry is served to applications
verbatim (no effective-set widening).

**I/Q formats** (bits 16-19):

| Flag                            | Bytes/sample | Description             |
|---------------------------------|-------------|-------------------------|
| `RIG_STREAM_FORMAT_IQ_CS8`     | 2           | Complex signed 8-bit    |
| `RIG_STREAM_FORMAT_IQ_CU8`     | 2           | Complex unsigned 8-bit  |
| `RIG_STREAM_FORMAT_IQ_CS16`    | 4           | Complex signed 16-bit   |
| `RIG_STREAM_FORMAT_IQ_CF32`    | 8           | Complex float 32-bit    |

For I/Q formats, one "sample" is one complex pair (I + Q components).

### Rules

1. **Don't mix audio and I/Q formats** in a single caps entry. Use
   separate entries for audio and I/Q stream types.
2. **I and Q are components of one channel, not two channels.** For a
   single-window receiver set `channels = { 1, 0 }`. Coherent
   multi-channel I/Q (several receivers sharing one LO/clock — e.g. diversity
   or X/Y polarization) is a *single* stream of N channel-interleaved complex
   samples: list every openable coherent count (the dummy backend
   advertises `{ 1, 2, 3, 4, 0 }`). Independent, non-coherent windows use
   separate streams.
   The `channels` list is 0-terminated, ascending and **exact**: every
   openable count is listed and none is implied. It need not be
   contiguous — a radio that opens 1 or 4 coherent channels but nothing
   in between declares `{ 1, 4, 0 }`, and exactly those two counts are
   openable; the frontend never fills the gap. (The one frontend-added
   convenience is audio-only: declaring either of mono/stereo makes the
   other openable through the mono↔stereo map.)
3. **List only what the hardware supports.** The frontend serves anything
   reachable from your native set through conversion and rejects the rest
   (`-RIG_EINVAL`); listing conversions yourself only mislabels them as
   hardware-native and breaks `require_native` for your users.
4. **List every native rate the hardware genuinely offers.** Format and
   channel conversion are always built into the frontend, but rate
   conversion depends on libsamplerate (optional at build time) — on a
   resampler-less build your declared rates are the only ones clients
   can open, so an omitted native rate is simply lost there.
5. **Terminate with `{ 0 }`.**
6. **Never write to `rig->caps`.** It is `/* read only */`, and one
   `rig_caps` is shared by every rig of the model — writing it corrupts
   the declaration for every other open rig and for the rest of the
   process, and `dump_caps` then reports the wreckage instead of the
   model. If your transport negotiates its format per connection, publish
   that with `stream_set_session_caps()` — see *Capabilities that depend
   on the connection* below.
7. **Check your work with `rigctl -m <model> --dump-caps`.** It prints
   the streaming section and warns about declaration mistakes: caps
   without the required `stream_open`/`stream_close` pair (and vice
   versa), a `stream_pause`/`stream_resume` override without its
   partner, `RIG_STREAM_CAP_HW_TIME` without `stream_hardware_time`,
   mixed audio/I-Q formats in one entry, non-ascending or empty
   rate/channel lists, duplicate (unreachable) type entries, and
   timed-TX flags on RX entries.

### Example

A network rig whose wire format is float32 audio at 48 kHz stereo, with
16-bit I/Q at 48-192 kHz. Declare exactly that — applications wanting
S16 audio or 44.1 kHz will be served through frontend conversion:

```c
/* At file scope, next to your rig_caps: */
static const struct rig_stream_caps mybackend_stream_caps[] =
{
    {
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .formats = RIG_STREAM_FORMAT_PCM_F32,     /* the wire format — only */
        .sample_rates = { 48000, 0 },
        .channels = { 1, 2, 0 },
        .max_streams = 1,
    },
    {
        .type = RIG_STREAM_TYPE_AUDIO_TX,
        .formats = RIG_STREAM_FORMAT_PCM_F32,
        .sample_rates = { 48000, 0 },
        .channels = { 1, 2, 0 },
        .max_streams = 1,
    },
    {
        .type = RIG_STREAM_TYPE_IQ_RX,
        .formats = RIG_STREAM_FORMAT_IQ_CS16,
        .sample_rates = { 48000, 96000, 192000, 0 },
        .channels = { 1, 0 },
        .max_streams = 1,
    },
    { 0 },  /* sentinel */
};

/* In your rig_caps: */
.stream_caps = mybackend_stream_caps,
```

A backend that builds caps at runtime (e.g. from a remote rig) fills a mutable
`static struct rig_stream_caps[]` buffer and assigns it — see
`rigs/dummy/netrigctl.c`. For the static case see `rigs/dummy/dummy.c`.

### Capabilities that depend on the connection

Some transports negotiate one audio geometry for a whole connection: a single
codec, rate and channel count, fixed at connect and unchangeable until the rig
is closed. The model declaration then describes what the *radio* can do across
configurations, which is wider than what *this* connection can carry.

That difference matters to the frontend, which chooses conversions from your
native declaration. If the declaration claims stereo is native while the
negotiated codec is mono, no channel conversion is installed and your mono
bytes are read as stereo — audio at the wrong speed, with every counter
reading zero. It matters to applications too: by rule 4, a rate you cannot
serve may be unreachable on a resampler-less build, so a client has to be able
to discover the connection's real geometry *before* it opens anything.

Declare the model's full capability in `rig_caps` as usual, then publish the
connection's truth from `rig_open`, once the negotiation is known:

```c
struct rig_stream_caps session[2];
int n = 0;

memset(session, 0, sizeof(session));
session[n].type = RIG_STREAM_TYPE_AUDIO_RX;
session[n].formats = RIG_STREAM_FORMAT_PCM_S16;   /* what was negotiated */
session[n].sample_rates[0] = negotiated_rate;
session[n].channels[0] = negotiated_channels;
session[n].max_streams = 1;
n++;

stream_set_session_caps(rig, session, n);
```

Entries are copied, so the array above may live on the stack. Publishing a
subset also *removes* what the connection cannot carry at all — a session
whose codec carries I/Q simply omits the audio entry, and vice versa.

The two views then answer different questions, and both stay honest:

| view | source | answers |
|---|---|---|
| `dump_caps` | `rig_caps`, never written | what this radio model can do |
| `rig_stream_caps_at()`, `\stream_caps` | session caps when published | what can be opened on this connection now |

Session caps gate the whole pipeline, not just discovery:
`rig_stream_open()` resolves requests against the same source the served
view derives from, so a configuration the session cannot carry is
refused even when the model declaration offers it.

Pass `NULL` to drop back to the model declaration. Backends whose format is a
protocol constant — the same for every connection — need none of this; see
`rigs/flexradio/smartsdr.c`, whose caps are `static const`. For a live
session-caps publisher see `rigs/dummy/netrigctl.c`, which publishes the
remote server's advertisement — its streaming capability exists only
per connection.

---

## 3. The `rig_stream` Handle

When the frontend calls your `stream_open` callback, it passes a fully
initialized `struct rig_stream *` (defined in `src/stream.h`). The ring
buffer is already allocated and active.

### Key fields

| Field               | Type                        | Description                          |
|---------------------|-----------------------------|--------------------------------------|
| `stream->type`      | `rig_stream_type_t`         | AUDIO_RX, AUDIO_TX, IQ_RX, or IQ_TX |
| `stream->config`    | `struct rig_stream_config`  | The APPLICATION's requested config   |
| `stream->backend_config` | `struct rig_stream_config` | The NATIVE config your backend runs at — **read this one**, not `config` (they differ on a converted stream) |
| `stream->conversions` | `int`                     | Active `RIG_STREAM_CONV_*` stages (informational; 0 = native) |
| `stream->ringbuf`   | `struct rig_stream_ringbuf` | Ring buffer for sample data          |
| `stream->backend_priv` | `void *`                 | Your per-stream state (you set this) |
| `stream->id`        | `int`                       | Unique ID within the stream type     |
| `stream->active`    | `HAMLIB_ATOMIC int`         | 1 while stream is open               |
| `stream->paused`    | `HAMLIB_ATOMIC int`         | 1 when paused by app/frontend        |
| `stream->muted`     | `HAMLIB_ATOMIC int`         | 1 when muted (zeros on read)         |
| `stream->center_freq` | `freq_t`                  | RF window center for panadapter I/Q backends (0 = unset; frontend then reports the VFO). See Section 10. |
| `stream->vfo`       | `vfo_t`                     | Associated VFO                       |
| `stream->gap_count` | `HAMLIB_ATOMIC int`         | Radio-side gaps you zero-filled (Section 5.1) |

### The `rig_stream_config`

```c
struct rig_stream_config {
    size_t struct_size;             /* set by rig_stream_config_alloc() */
    rig_stream_type_t type;
    rig_stream_format_t format;     /* Single format value (not a bitmask) */
    int sample_rate;
    int channels;
    int frame_samples;                 /* Samples per frame (0 = backend default) */
    size_t buffer_bytes;            /* Ring buffer capacity (0 = default) */
    unsigned int buffer_duration_ms; /* If set and buffer_bytes==0, derive size */
    unsigned int time_stale_coarse_ms;     /* Staleness watchdog (0 = default) */
    unsigned int time_stale_invalidate_ms; /* Staleness watchdog (0 = default) */
    unsigned int mtu;                      /* Sender path MTU (0 = default 1500) */
};
```

Backends receive an already-populated `stream->config`; they never allocate
it. Applications, however, MUST obtain the config from
`rig_stream_config_alloc()` and release it with `rig_stream_config_free()`
(never stack-allocate it) so appended fields such as `mtu` stay
ABI-compatible — see HAMLIB_STREAMING.md §4 "Object ownership & lifetimes".

Defaults: 64 KB for audio buffers, 4 MB for I/Q buffers
(`RIG_STREAM_AUDIO_BUF_DEFAULT` / `RIG_STREAM_IQ_BUF_DEFAULT`).

### What the frontend handles for you

- Validates config against your `stream_caps` before calling `stream_open`
- **Format, rate and channel conversion**: resolves the requested config to
  a native source from your caps, fills `stream->backend_config` with it,
  and runs the conversion pipeline on the producer side of the ring — you
  produce/consume native data only (deliver RX bytes with
  `stream_backend_write()`, Section 5). Backend-domain sample counts
  (time anchors, gap accounting) are rescaled for the application
- Allocates and initializes the ring buffer (sized in the CONSUMER's
  format: the request on RX, your native side on TX)
- Manages the slot table (max concurrent streams)
- Tracks overrun/underrun counts
- Handles mute (returns zeros to app on read, discards on write)


---

## 4. Implement `stream_open`

```c
int (*stream_open)(RIG *rig, struct rig_stream *stream);
```

When called, the stream handle is ready. Your job: set up per-stream state
and start producing (RX) or consuming (TX) data.

### Steps

1. Access your backend's private data
2. Allocate a per-stream state struct
3. Store it in `stream->backend_priv`
4. For RX streams: start a producer thread
5. For TX streams: optionally start a consumer thread
6. Return `RIG_OK` or an error code

Use `stream_type_is_rx()` and `stream_type_is_iq()` from `stream_proto.h`
for stream direction/type classification.

### Example

```c
int myrig_stream_open(RIG *rig, struct rig_stream *stream)
{
    struct myrig_priv *priv = (struct myrig_priv *)STATE(rig)->priv;

    struct myrig_stream_state *ss = calloc(1, sizeof(*ss));

    if (!ss)
    {
        return -RIG_ENOMEM;
    }

    ss->stream = stream;
    ss->running = 1;
    ss->radio_sock = -1;

    /* Tell the radio to start streaming (radio-specific protocol).
     * backend_config: the native side of any conversion. */
    int ret = myrig_start_hw_stream(priv, stream->backend_config.type,
                                     stream->backend_config.sample_rate,
                                     &ss->radio_sock);

    if (ret < 0)
    {
        free(ss);
        return -RIG_EIO;
    }

    stream->backend_priv = ss;

    if (stream_type_is_rx(stream->type))
    {
        int err = pthread_create(&ss->thread, NULL, myrig_rx_thread, ss);

        if (err != 0)
        {
            myrig_stop_hw_stream(priv, ss->radio_sock);
            free(ss);
            stream->backend_priv = NULL;
            return -RIG_EIO;
        }
    }

    return RIG_OK;
}
```

**Error handling:** Free everything on failure. Set `backend_priv = NULL`
if it was already assigned.

See also: `dummy_stream_open()` in `rigs/dummy/dummy_stream.c`.


---

## 5. The RX Producer Thread

The RX thread receives data from hardware and delivers it — in your
**native** format, at the `stream->backend_config` rate — through
`stream_backend_write()`, which applies the frontend conversion pipeline
(if one is installed) and writes the ring buffer.

### Thread loop pattern

```c
static void *myrig_rx_thread(void *arg)
{
    struct myrig_stream_state *ss = (struct myrig_stream_state *)arg;
    struct rig_stream *stream = ss->stream;
    /* backend_config, not config: the native side of any conversion */
    const struct rig_stream_config *cfg = &stream->backend_config;
    int sample_size = rig_stream_format_sample_size(cfg->format);
    int frame_samples = cfg->frame_samples > 0 ? cfg->frame_samples : 480;
    size_t frame_bytes = frame_samples * cfg->channels * sample_size;
    unsigned char *buf = malloc(frame_bytes);

    if (!buf)
    {
        return NULL;
    }

    while (ss->running)
    {
        if (stream->paused)
        {
            usleep(10000);  /* 10 ms idle when paused */
            continue;
        }

        /* Receive from radio (blocking with timeout) */
        ssize_t got = recv(ss->radio_sock, buf, frame_bytes, 0);

        if (got <= 0)
        {
            continue;  /* Timeout or error — retry */
        }

        stream_backend_write(stream, buf, (size_t)got);
    }

    free(buf);
    return NULL;
}
```

### Key points

- **Read `stream->backend_config`, deliver with `stream_backend_write()`.**
  On a native stream it is byte-for-byte `stream_ringbuf_write()`; on a
  converted stream it runs the pipeline first. Writing the ring directly
  would bypass conversion and corrupt a converted stream.
- **`stream_backend_write()` never blocks.** If the buffer is full, oldest data
  is overwritten and `overrun_count` is incremented.
- **Check `stream->paused`** in the loop. When paused, sleep briefly and
  skip data production.
- **Frame timing:** For network radios, timing is implicit — the blocking
  `recv()` call paces the thread. For local devices, sleep between writes:
  ```c
  long sleep_ns = (long)((double)frame_samples / sample_rate * 1e9);
  ```
- **Buffer sizing:** Use `rig_stream_format_sample_size()` to compute
  bytes per sample. For I/Q formats, this returns the size of one complex
  pair (e.g., 4 bytes for CS16, 8 bytes for CF32).
- **Network recv timeout:** Set `SO_RCVTIMEO` on the socket so the thread
  can check `ss->running` and exit promptly when the stream closes.

See also: `dummy_stream_generator()` in `rigs/dummy/dummy_stream.c`.

### 5.1 Gap handling

The ring buffer is a byte stream — it cannot mark a discontinuity by
itself. If your radio protocol lets you detect lost packets (a
sequence-number skip or a timestamp jump), report the loss with
`rig_stream_mark_gap()` so the application sees an exact hole:

```c
/* gap_samples = missing samples computed from the radio protocol;
 * pass 0 when the protocol cannot size the gap. Call BEFORE writing
 * the post-gap data. */
rig_stream_mark_gap(stream, gap_samples);
```

This advances the producer sample-index domain without writing bytes, so
the loss reaches the application through `rig_stream_read()`'s
`info.dropped_samples` + `RIG_STREAM_DROP_GAP` — exactly like a ring
overrun — and feeds the per-cause totals in `rig_stream_get_stats()`.
Follow the mark with a DISCONTINUITY time anchor (Section 5.2).

For **audio**, additionally zero-fill the ring buffer so live playback
stays smooth (the zeros are real samples, so `dropped_samples` stays 0;
the DISCONTINUITY anchor still tells recorders they are synthetic). Cap
the fill (e.g. to one second) so a clock jump cannot flood the buffer:

```c
memset(silence, 0, chunk_bytes);
/* write gap_samples worth of zeros (in chunks) */
stream_backend_write(stream, silence, gap_samples * frame_bytes);
stream->gap_count++;
```

For **I/Q**, do not zero-fill — a mis-sized fill shifts phase and
corrupts downstream FFT processing; `mark_gap` gives the application the
exact position and size so it can choose its own policy. When the gap
size cannot be trusted (e.g. the FlexRadio DAX-IQ fractional counter),
use `rig_stream_mark_gap(stream, 0)` — an unsized, position-only report.


### 5.2 Providing capture time

Push a time anchor (`sample_index ↔ UTC`) so the enriched read can
report absolute capture time:

```c
struct rig_stream_time_anchor anchor;
memset(&anchor, 0, sizeof(anchor));
anchor.sample_index = my_native_samples_produced;  /* YOUR native count */
stream_time_now(&anchor.seconds, &anchor.picoseconds);  /* host clock */
anchor.source = RIG_STREAM_TIME_SRC_HOST;
anchor.accuracy = RIG_STREAM_TIME_ACC_MS;
rig_stream_push_time_anchor(stream, &anchor);
```

Rules:

1. **`sample_index` is in your NATIVE sample domain** — the count of
   samples your producer has delivered (plus gap samples), at the
   `backend_config` rate. The frontend rescales it into the
   application's domain when rate conversion is active. Do NOT use
   `rig_stream_get_samples_written()` as the index: it reports the
   already-rescaled consumer-domain position and would be rescaled a
   second time. Keep your own counter (see `native_pos` in
   `dummy_stream_generator()`). The same rule applies to
   `rig_stream_mark_gap()` counts — report losses in native samples.
2. Push at stream start and **at least every second** (the read-path
   staleness watchdog degrades and then invalidates older time).
3. Push immediately after any detected gap, with
   `RIG_STREAM_TIME_FLAG_DISCONTINUITY` set.
4. When the radio supplies absolute time tied to its sample clock (e.g.
   GPSDO VITA-49 `TSI=UTC`), use it: set the radio-derived
   seconds/picoseconds, `source = RIG_STREAM_TIME_SRC_GPS`,
   `RIG_STREAM_TIME_FLAG_SAMPLE_REFERENCED | RIG_STREAM_TIME_FLAG_LOCKED`, and an
   honest accuracy class — and stamp every packet. Host-arrival clocks
   must NOT claim `SAMPLE_REFERENCED`.

Conversion helpers (ns/timespec ↔ seconds+picoseconds, sample
interpolation) live in `src/stream_time.h`.

The TX-side counterpart — scheduling bursts against this same time
base — is Section 6.1.


---

### 5.3 Codec-frame streams

A compressed format (e.g. `OPUS`) streams **whole codec frames**, not
sample bytes, and the frontend applies no conversion to them. The
producer/consumer contract changes accordingly:

- Produce RX frames with
  `stream_backend_write_frame(stream, buf, len, duration_samples)` — one
  call per codec frame, `len` up to the stream's `max_payload`. Never
  blocks: a full ring drops the NEWEST frame (counted as an overrun with
  its duration). Do **not** use `stream_backend_write()` on a codec
  stream — it refuses.
- **You supply the decoded duration** (samples per frame at the native
  rate): from your protocol's fixed cadence — e.g. FlexRadio's radio-side
  Opus is one 10 ms frame per packet, 240 samples at 24 kHz — or by
  parsing the codec's in-band self-description (the Opus TOC byte gives
  the duration without any decoding). Pass 0 if genuinely unknown; the
  stream's timing features then degrade.
- Frame start indexes accumulate automatically from the durations; report
  radio-side losses with `rig_stream_mark_gap()` in decoded samples as
  usual, and the hole surfaces to consumers as a start-index jump.
- Consume TX frames with
  `stream_backend_read_frame(stream, buf, cap, &len, &duration,
  &start_index, timeout_ms)` — one whole frame per call; a cap of
  `max_payload` bytes always suffices.
- Declare codec formats in `stream_caps` exactly as the hardware offers
  them (rate and channels must match the declaration exactly at open —
  codec requests are native-only, Section 2).

The dummy backend's fabricated OPUS (deterministic variable-length frames)
is the reference implementation and the system-test vehicle:
`dummy_stream_generator()` (produce), the codec branch of
`dummy_stream_tx_scheduler()` (consume) and the record-verbatim codec
loopback.


---

## 6. The TX Consumer Thread

For TX streams, the application writes data via `rig_stream_write()`,
which goes into the ring buffer. The backend reads from it and sends to
the radio.

### Two approaches

**Thread-based** (recommended for network radios):

```c
static void *myrig_tx_thread(void *arg)
{
    struct myrig_stream_state *ss = (struct myrig_stream_state *)arg;
    struct rig_stream *stream = ss->stream;
    /* backend_config: the TX ring holds your NATIVE format — the frontend
     * converted the application's data before enqueueing it. */
    const struct rig_stream_config *cfg = &stream->backend_config;
    int sample_size = rig_stream_format_sample_size(cfg->format);
    int frame_samples = cfg->frame_samples > 0 ? cfg->frame_samples : 480;
    size_t frame_bytes = frame_samples * cfg->channels * sample_size;
    unsigned char *buf = malloc(frame_bytes);

    if (!buf)
    {
        return NULL;
    }

    while (ss->running)
    {
        if (stream->paused)
        {
            usleep(10000);
            continue;
        }

        /* Read from ring buffer, blocking up to 100 ms */
        size_t got = stream_ringbuf_read(&stream->ringbuf, buf, frame_bytes, 100);

        if (got == 0)
        {
            continue;  /* Timeout — check running flag and retry */
        }

        /* Send to radio */
        send(ss->radio_sock, buf, got, 0);
    }

    free(buf);
    return NULL;
}
```

**Callback-based** (simpler, for tight coupling with hardware):

Implement the `stream_write` callback in `rig_caps` to send data directly
to the radio, bypassing the ring buffer. The frontend calls your callback
instead of writing to the ring buffer.

### Key points

- **`stream_ringbuf_read()` blocks** up to `timeout_ms` waiting for data. Returns
  0 on timeout, incrementing `underrun_count`.
- **Use a short timeout** (e.g., 100 ms) so the thread checks `ss->running`
  regularly and can exit when the stream closes.
- **Shutdown:** `stream_close` sets `running = 0`. The thread wakes from
  `stream_ringbuf_read()` timeout and exits.

See also: `dummy_stream_loopback_thread()` in `rigs/dummy/dummy_stream.c`
for the ring buffer read pattern.


### 6.1 Timed transmit and burst PTT

If your TX hardware or play-out loop can honor "start at UTC instant T",
declare it in the caps entry and drain burst targets from your TX
thread:

```c
/* caps entry */
.caps_flags = RIG_STREAM_CAP_TIMED_TX_COARSE | RIG_STREAM_CAP_BURST_PTT,
.tx_schedule_horizon_ms = 30000,

/* TX thread, before consuming each frame. The second argument is a
 * sample-position watermark: pop every target due at or before the
 * frame you are about to emit (consumed = samples already sent). */
struct rig_stream_time_anchor tgt;

while (rig_stream_pop_tx_target(stream, consumed + frame_samples, &tgt))
{
    if (tgt.flags & RIG_STREAM_TIME_FLAG_TX_TIMED)
    {
        /* wait until the target instant; if past due beyond your
         * tolerance, transmit immediately and report it late */
        ... gate ...;
        struct rig_stream_write_status ev;
        memset(&ev, 0, sizeof(ev));
        ev.event = RIG_STREAM_WRITE_EVENT_LATE;
        ev.sample_index = consumed;
        ev.lateness = late_samples;          /* how late, in samples */
        ev.time_valid = 1;
        ev.seconds = tgt.seconds;
        ev.picoseconds = tgt.picoseconds;
        stream_record_write_status(stream, &ev, 0 /* local */);
    }

    if (burst_ptt && (tgt.flags & RIG_STREAM_TIME_FLAG_SOB)) { /* key PTT */ }
    if (burst_ptt && (tgt.flags & RIG_STREAM_TIME_FLAG_EOB)) { /* unkey */ }
}
```

The frontend validates timed writes against your `caps_flags` and
`tx_schedule_horizon_ms` before they reach the target channel. Declare
`RIG_STREAM_CAP_BURST_PTT` only when SOB/EOB should drive PTT; otherwise
the application manages PTT itself. See `dummy_stream_tx_scheduler()` in
`rigs/dummy/dummy_stream.c` for the reference implementation.


### 6.2 Reporting async TX problems

A backend reports a late burst or a TX ring under/overrun by recording a
**write-status event**: `stream_record_write_status(stream, &ev, remote)`.
This bumps the matching stat counter and queues the event for the application's
`rig_stream_wait_write_status()` (and, over rigctld, forwards it to a netrigctl
client as a `WRITE_STATUS` frame). Pass `remote = 0` for an event on your own
hardware/host. Fill `event`, `sample_index`, and — where you know them —
`dropped_samples` (under/overrun), `lateness` (late), and the UTC time fields.
It is TX-only; **RX losses use `rig_stream_mark_gap()`** instead (Section 5.1),
which the application sees inline via `rig_stream_read_info`. A local TX ring
overrun on `rig_stream_write()` is reported for you by the frontend; you only
record events your own thread detects (a late burst, a consumer-side underrun).


---

## 7. Implement `stream_close`

```c
int (*stream_close)(RIG *rig, struct rig_stream *stream);
```

Stop all backend activity for this stream and free per-stream state. The
frontend handles ring buffer destruction after this returns.

### Steps

1. Retrieve per-stream state from `stream->backend_priv`
2. Set `running = 0`
3. Join threads (`pthread_join`)
4. Close radio-side sockets/connections for this stream
5. Unregister from backend tracking
6. `free()` the per-stream state
7. Set `stream->backend_priv = NULL`

### Example

```c
int myrig_stream_close(RIG *rig, struct rig_stream *stream)
{
    struct myrig_priv *priv = (struct myrig_priv *)STATE(rig)->priv;
    struct myrig_stream_state *ss =
        (struct myrig_stream_state *)stream->backend_priv;

    if (!ss)
    {
        return RIG_OK;
    }

    ss->running = 0;

    /* Join producer (RX) or consumer (TX) thread */
    if (ss->thread_started)
    {
        pthread_join(ss->thread, NULL);
    }

    /* Tell radio to stop this stream */
    myrig_stop_hw_stream(priv, ss->radio_sock);

    stream->backend_priv = NULL;
    free(ss);

    return RIG_OK;
}
```

**Thread exit timing:** The thread must exit promptly when `running` is
cleared. Network `recv()` calls must use timeouts — if the thread blocks
indefinitely, `pthread_join` will hang. Set `SO_RCVTIMEO` on receive
sockets.

See also: `dummy_stream_close()` in `rigs/dummy/dummy_stream.c`.


---

## 8. Register Callbacks in rig_caps

Add the function pointer assignments to your `rig_caps` struct:

```c
.stream_open  = myrig_stream_open,
.stream_close = myrig_stream_close,
```

### Optional callbacks

| Callback                | When to implement                                | Default if NULL                                |
|-------------------------|--------------------------------------------------|------------------------------------------------|
| `.stream_read`          | Direct hardware DMA, bypassing ring buffer read  | Read from `stream->ringbuf`                    |
| `.stream_write`         | Direct-to-hardware send, bypassing ring buffer   | Write to `stream->ringbuf`                     |
| `.stream_drain`         | Custom TX drain logic (hardware FIFO flush)      | Poll the ring buffer until empty or timeout    |
| `.stream_pause`         | Hardware-level pause (tell radio to stop sending) | Set `stream->paused`; frontend withholds reads |
| `.stream_resume`        | Hardware-level resume                            | Clear `stream->paused`                         |
| `.stream_apply_metadata`| Timed TX metadata (frequency change at a sample) | `rig_set_freq()`/`rig_set_vfo()`/PTT on stream VFO |
| `.stream_hardware_time` | Radio sample-clock timestamp (GPS/OCXO disciplined) | Host `CLOCK_REALTIME` (`RIG_STREAM_TIME_SRC_HOST`) |

**Signatures** (from `include/hamlib/rig.h`):

```c
int (*stream_read)(RIG *rig, struct rig_stream *stream,
                   void *buffer, size_t buffer_size,
                   size_t *bytes_read, int timeout_ms,
                   struct rig_stream_read_info *info);
int (*stream_write)(RIG *rig, struct rig_stream *stream,
                    const void *buffer, size_t buffer_size,
                    size_t *bytes_written, int timeout_ms,
                    const struct rig_stream_write_info *info);
int (*stream_drain)(RIG *rig, struct rig_stream *stream, int timeout_ms);
int (*stream_pause)(RIG *rig, struct rig_stream *stream);
int (*stream_resume)(RIG *rig, struct rig_stream *stream);
int (*stream_apply_metadata)(RIG *rig, struct rig_stream *stream,
                             const struct rig_stream_metadata *meta);
int (*stream_hardware_time)(RIG *rig, struct rig_stream *stream,
                            struct rig_stream_time_anchor *now);
```

If you don't implement `stream_read` / `stream_write`, the frontend reads
from / writes to the ring buffer directly — this is the common case.

**`stream_drain` is a TX-drain operation.** The default only polls the local
ring buffer empty, so any backend whose transmitted samples live somewhere the
ring buffer doesn't — a hardware TX FIFO, or a remote radio reached over a
custom transport — should override it to drain that real buffer. A backend that
overrides `stream_write` to send TX directly (bypassing the ring buffer) makes
the default drain a no-op and therefore *must* override `stream_drain` too.
`netrigctl` does this by forwarding `\stream_drain` to the daemon, which runs
the real backend's flush (see `netrigctl_stream_drain` in
`rigs/dummy/netrigctl.c`).

See also: the `.stream_open` / `.stream_close` assignments in
`rigs/dummy/dummy.c` for callback registration.


---

## 9. Format Conversion Utilities

**You normally don't need these.** When the application's requested
format, rate or channel count differs from your native declaration, the
frontend installs the conversion itself (Sections 2-3) — backends no
longer hand-roll per-client conversion. The stateless utilities in
`src/stream_convert.h` remain for the cases that stay backend-side: the
device-codec chain (Section 9.1) and internal processing on your own
native data.

### Sample size

```c
int rig_stream_format_sample_size(rig_stream_format_t format);
```

Returns bytes per sample (per complex pair for I/Q). Returns 0 for
compressed formats (Opus).

| Format      | Audio size | I/Q size |
|-------------|-----------|----------|
| S8 / U8     | 1         | —        |
| S16         | 2         | —        |
| F32         | 4         | —        |
| IQ_CU8/CS8  | —         | 2        |
| IQ_CS16     | —         | 4        |
| IQ_CF32     | —         | 8        |

### Format conversion

```c
int rig_stream_convert(const void *src, rig_stream_format_t src_format,
                       void *dst, rig_stream_format_t dst_format,
                       size_t sample_count, int channels);
```

Converts between any two audio formats or any two I/Q formats. Cannot
cross between audio and I/Q families. `sample_count` is samples per
channel (or complex pairs for I/Q).

### Channel conversion

```c
int rig_stream_convert_channels(const void *src, int src_channels,
                                void *dst, int dst_channels,
                                size_t sample_count,
                                rig_stream_format_t format);
```

Mono→stereo duplicates each sample. Stereo→mono averages L and R.

### Sample rate conversion

```c
int rig_stream_resample(const float *src, int src_rate,
                        float *dst, int dst_rate,
                        size_t src_samples, size_t *dst_samples,
                        int channels, int quality);
```

F32 only. Requires libsamplerate (returns -1 if not available).
Quality: `RIG_RESAMPLE_BEST`, `RIG_RESAMPLE_MEDIUM`, `RIG_RESAMPLE_FAST`.

### Recommended conversion order

1. **Channels** first (reduces data volume if downmixing)
2. **Format** second
3. **Resample** last

(The frontend's own per-stream pipeline follows the same order — channel
map → float pivot → stateful resampler → destination format — with a
persistent resampler state per stream, which the stateless
`rig_stream_resample()` above cannot provide across chunk boundaries.
That is another reason to leave app-facing conversion to the frontend.)

### 9.1 Device audio codecs (μ-law, A-law, ADPCM)

If the radio carries audio over the device link in a companded or
compressed codec rather than raw PCM, terminate it with
`src/stream_codec.{c,h}` before the steps above. Open one codec state per
direction (it holds state for stateful codecs such as ADPCM):

```c
stream->rx_codec = rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1 /* mono */);
```

The codec call takes a caller-chosen PCM `pcm_format` — pass your
**native** format from `stream->backend_config` (the codec performs that
format hop internally); everything app-facing is the frontend's job.

```c
/* RX: device codec bytes -> native PCM */
uint8_t  radio_buf[FRAME_BYTES];
float    pcm_buf[FRAME_SAMPLES];
size_t   pcm_bytes = 0;

ssize_t got = recv(sock, radio_buf, sizeof(radio_buf), 0);

rig_audio_convert_to_pcm(stream->rx_codec, radio_buf, (size_t)got,
                         RIG_STREAM_FORMAT_PCM_F32,   /* = native format */
                         pcm_buf, sizeof(pcm_buf), &pcm_bytes);
stream_backend_write(stream, pcm_buf, pcm_bytes);

/* TX: native PCM (from the ring) -> device codec bytes */
uint8_t  dev_buf[FRAME_BYTES];
size_t   dev_bytes = 0;
rig_audio_convert_from_pcm(stream->tx_codec, RIG_STREAM_FORMAT_PCM_F32,
                           pcm_buf, pcm_bytes,
                           dev_buf, sizeof(dev_buf), &dev_bytes);
send(sock, dev_buf, dev_bytes, 0);
```

At a stream gap (§5.1), reset the affected RX codec state **before**
decoding post-gap data, so a stateful codec's predictor does not carry
across the discontinuity (stateless codecs ignore the reset):

```c
rig_stream_mark_gap(stream, dropped_samples);
rig_audio_codec_reset(stream->rx_codec);
```

Size output buffers with `rig_audio_codec_max_pcm_bytes()` /
`rig_audio_codec_max_encoded_bytes()` rather than assuming a ratio. A
backend whose device link is already PCM uses `RIG_AUDIO_CODEC_NONE` (or
skips the codec stage and calls `rig_stream_convert()` directly). Close
both codec states in `stream_close`.


---

## 10. Metadata Integration

Metadata provides time-aligned rig state (frequency, VFO, PTT) associated
with the stream's sample data.

### How it works

The frontend reads metadata from `rig_cache` automatically via
`rig_stream_read_metadata()`. **No backend work is required for
basic metadata** — a working CAT backend already keeps `rig_cache` up to
date via `rig_set_cache_freq()`, `rig_set_cache_mode()`, etc.

### The metadata struct

```c
struct rig_stream_metadata {
    uint32_t field_mask;    /* Which fields are valid (RIG_STREAM_META_*) */
    uint64_t sample_index;  /* Sample index this metadata applies to */
    uint8_t  vfo_id;        /* VFO identifier */
    uint8_t  ptt;           /* PTT state (0=RX, 1=TX) */
    freq_t   center_freq;   /* RF center of the I/Q window in Hz (I/Q streams) */
    freq_t   vfo_freq;      /* primary demod / dial frequency in Hz */
    uint64_t _reserved[4];  /* ABI headroom; callers SHOULD zero-init the struct */
};
```

Field mask bits: `RIG_STREAM_META_VFO_ID`, `RIG_STREAM_META_PTT`,
`RIG_STREAM_META_CENTER_FREQ`, `RIG_STREAM_META_VFO_FREQ`. For I/Q streams
the frontend fills `center_freq` from the VFO frequency by default; a
backend that tunes the window independently of the dial (panadapter
DAX-IQ) sets `stream->center_freq` to the true window center and the
frontend reports that instead.

### Optional: `stream_apply_metadata`

For TX streams, a client can send metadata with a frequency or PTT change
tied to a specific sample timestamp. If you implement
`stream_apply_metadata`, the frontend calls it when the application writes
metadata. Use this for radios that support timed frequency changes
synchronized to the sample stream.

If not implemented, the frontend falls back to standard `rig_set_freq()`
calls.


---

## 11. Per-Stream Private State Design

Your per-stream state struct is allocated in `stream_open`, stored in
`stream->backend_priv`, and freed in `stream_close`.

### Required fields

```c
struct myrig_stream_state
{
    struct rig_stream *stream;      /* Back-pointer to stream handle */
    pthread_t thread;               /* Producer (RX) or consumer (TX) thread */
    HAMLIB_ATOMIC int running;      /* Thread run flag (0 = stop) */
    int radio_sock;                 /* Radio-side socket for this stream */
};
```

### Optional fields

- **Conversion buffers** — pre-allocated if format conversion is needed
- **Sequence numbers** — for gap detection in radio protocol
- **Protocol state** — radio-specific stream ID, channel assignment, etc.
- **Thread-started flag** — if the thread is conditionally created

### Backend-level tracking

Track active streams in your backend's `priv` struct for cleanup and
pairing (e.g., TX→RX loopback):

```c
struct myrig_priv
{
    /* ... existing fields ... */
    struct myrig_stream_state *streams[RIG_STREAM_TYPE_COUNT];
};
```

See also: `struct dummy_stream_state` in `rigs/dummy/dummy_stream.h`.


---

## 12. Build System Integration

### Makefile.am

Add your streaming source files to the backend's `_SOURCES`:

```makefile
libhamlib_myrig_la_SOURCES = myrig.c myrig.h myrig_stream.c myrig_stream.h
```

### Headers to include

```c
#include "stream.h"           /* struct rig_stream, ringbuf API */
#include "stream_convert.h"   /* rig_stream_convert(), sample_size() */
#include "stream_proto.h"     /* stream_type_is_rx(), format names */
```

### Link dependencies

- `-lpthread` — already in Hamlib's standard deps
- `-lm` — if using math functions (sin/cos for test tones)
- libsamplerate — optional, for `rig_stream_resample()`. Guarded by
  `HAVE_SAMPLERATE` / `SAMPLERATE_LIBS` in configure


---

## 13. Testing Your Backend

Use the acutest framework (`test/acutest.h`) for unit and integration
tests.

### Basic test pattern

```c
#include "acutest.h"
#include <hamlib/rig.h>
#include "stream.h"

void test_rx_audio(void)
{
    RIG *rig = rig_init(RIG_MODEL_MYRIG);
    TEST_ASSERT(rig != NULL);
    TEST_CHECK(rig_open(rig) == RIG_OK);

    struct rig_stream_config *cfg = rig_stream_config_alloc();
    TEST_ASSERT(cfg != NULL);
    cfg->type = RIG_STREAM_TYPE_AUDIO_RX;
    cfg->format = RIG_STREAM_FORMAT_PCM_S16;
    cfg->sample_rate = 48000;
    cfg->channels = 1;

    rig_stream_t *stream = NULL;
    TEST_CHECK(rig_stream_open(rig, cfg, &stream) == RIG_OK);
    rig_stream_config_free(cfg);  /* stream kept its own copy */
    TEST_ASSERT(stream != NULL);

    int16_t buf[4800];  /* 100 ms at 48 kHz */
    size_t got = 0;
    TEST_CHECK(rig_stream_read(rig, stream, buf, sizeof(buf),
                                &got, 1000, NULL) == RIG_OK);
    TEST_CHECK(got > 0);

    TEST_CHECK(rig_stream_close(rig, stream) == RIG_OK);
    rig_close(rig);
    rig_cleanup(rig);
}
```

### What to test

- **Capabilities:** `rig_stream_caps_count()` / `rig_stream_caps_at()` return the expected entries
- **RX data flow:** Open RX stream, read data, verify non-zero bytes
- **TX data flow:** Open TX stream, write data, verify no errors
- **Format variations:** Test each format declared in `stream_caps`
- **Open/close cycles:** Repeated open/close, no leaks or corruption
- **Overrun detection:** Open RX, delay reading, verify
  `rig_stream_get_stats()` reports `overruns > 0`
- **Gap reporting:** If the radio protocol detects losses, verify
  `info.dropped_samples`/`drop_flags` and the stats totals
- **Capture time:** Read with `info` and verify `time_valid`, source,
  and accuracy match what your backend anchors claim
- **Metadata:** Verify `rig_stream_read_metadata()` returns valid
  data with correct frequency

### rigctld integration

Once your backend works through the C API, rigctld streaming works
automatically — rigctld calls the same `rig_stream_open/close` hooks and
feeds data over UDP to remote clients.

See also: `test/test_dummy_stream.c` for the complete worked-example
test suite.


---

## 14. Implementation Checklist

```
[ ] 1. Identify hardware streaming capabilities
      (formats, sample rates, channels, max concurrent streams)
[ ] 2. Add stream_caps array to rig_caps (Section 2)
[ ] 3. Define per-stream state struct (Section 11)
[ ] 4. Implement stream_open (Section 4)
      [ ] 4a. Allocate per-stream state → stream->backend_priv
      [ ] 4b. For RX: start producer thread
      [ ] 4c. For TX: start consumer thread (if needed)
      [ ] 4d. Clean up on error paths
[ ] 5. Implement RX producer thread (Section 5)
      [ ] 5a. Receive/generate data in your NATIVE format
              (stream->backend_config)
      [ ] 5b. Call stream_backend_write() with frame data
      [ ] 5c. Handle paused flag
      [ ] 5d. Exit when running flag cleared
[ ] 6. Implement TX consumer thread (Section 6) — if needed
      [ ] 6a. Call stream_ringbuf_read() with timeout
      [ ] 6b. Send data to radio hardware
      [ ] 6c. Handle paused flag and clean shutdown
[ ] 7. Implement stream_close (Section 7)
      [ ] 7a. Set running = 0
      [ ] 7b. pthread_join thread(s)
      [ ] 7c. Close radio-side resources
      [ ] 7d. Free per-stream state, set backend_priv = NULL
[ ] 8. Register callbacks in rig_caps (Section 8)
[ ] 9. Add source files to Makefile.am (Section 12)
[ ] 10. Wire a device codec if the link is companded/compressed (Section 9.1)
        — app-facing format conversion is the frontend's job, not yours
[ ] 10b. Radio emits/accepts encoded codec frames (e.g. Opus)? Declare the
        codec format in stream_caps and use the codec-frame produce/consume
        API with producer-supplied durations (Section 5.3)
[ ] 11. Implement optional callbacks if needed (Section 8)
[ ] 12. Write and run tests (Section 13)
[ ] 13. Verify metadata via rig_stream_read_metadata() (Section 10)
[ ] 14. Test via rigctld: stream_caps, stream_open, UDP data flow
```


---

## 15. Quick Reference

### Ring buffer API (`src/stream.h`)

| Function             | Description                                      |
|----------------------|--------------------------------------------------|
| `stream_backend_write(stream, data, len)` | Write native data format to ring buffer: native bytes in, conversion applied, ring written - RX producers should always use this |
| `stream_ringbuf_write(rb, data, len)` | Write to buffer (never blocks, overwrites oldest) — bypasses conversion |
| `stream_backend_write_frame(stream, buf, len, dur)` | Codec streams: enqueue ONE codec frame with its decoded duration (Section 5.3) |
| `stream_backend_read_frame(stream, buf, cap, ...)` | Codec TX consume: dequeue ONE whole codec frame with metadata |
| `stream_ringbuf_read(rb, data, len, timeout_ms)` | Read with timeout (blocks if empty; a timeout counts an underrun only once the ring has ever been fed) |
| `stream_ringbuf_available(rb)` | Bytes available to read                        |
| `stream_ringbuf_reset(rb)`  | Reset to empty                                   |

### Backend time duties (`src/stream.h`, `src/stream_time.h`)

| Function                          | Description                       |
|-----------------------------------|-----------------------------------|
| `rig_stream_push_time_anchor()`   | Record a capture-time anchor (Section 5.2) |
| `rig_stream_mark_gap()`           | Report a radio-side loss (Section 5.1) |
| `rig_stream_pop_tx_target()`      | Drain pending TX burst targets (Section 6.1) |
| `stream_record_write_status()`    | Report a TX late burst / under- / overrun (Section 6.2) |
| `stream_time_now()`               | Host CLOCK_REALTIME as sec+ps     |
| `stream_time_add_samples()`       | Advance a time by N sample periods |

### Frontend API (`include/hamlib/rig.h`)

| Function                        | Description                        |
|---------------------------------|------------------------------------|
| `rig_stream_config_alloc()` / `rig_stream_config_free()` | Allocate/free a config (apps use these; never stack-allocate) |
| `rig_stream_caps_count()` / `rig_stream_caps_at()` | Query backend streaming capabilities |
| `rig_stream_open()`             | Open stream (calls backend hook)   |
| `rig_stream_close()`            | Close stream                       |
| `rig_stream_read()`             | Read from RX stream                |
| `rig_stream_write()`            | Write to TX stream                 |
| `rig_stream_drain()`            | Wait for TX data to be consumed    |
| `rig_stream_pause()`            | Pause stream I/O                   |
| `rig_stream_resume()`           | Resume stream I/O                  |
| `rig_stream_mute()`             | Mute (zeros on read, discard write)|
| `rig_stream_unmute()`           | Unmute                             |
| `rig_stream_get_stats()`        | Health snapshot: per-cause event counts + lost-sample totals |
| `rig_stream_get_time_anchor()`  | Latest capture-time anchor         |
| `rig_stream_get_hardware_time()`| Radio clock + discipline (host fallback) |
| `rig_stream_get_samples_written()`  | Producer sample position           |
| `rig_stream_read_metadata()` | Read metadata from rig cache    |
| `rig_stream_write_metadata()`   | Apply TX metadata                  |

### Helper inlines (`src/stream_proto.h`)

| Function              | Returns true for                  |
|-----------------------|-----------------------------------|
| `stream_type_is_rx()` | `AUDIO_RX`, `IQ_RX`              |
| `stream_type_is_tx()` | `AUDIO_TX`, `IQ_TX`              |
| `stream_type_is_iq()` | `IQ_RX`, `IQ_TX`                 |

### Conversion API (`src/stream_convert.h`)

| Function                        | Description                        |
|---------------------------------|------------------------------------|
| `rig_stream_format_sample_size()` | Bytes per sample (0 for compressed)|
| `rig_stream_convert()`          | Format conversion (audio or I/Q)   |
| `rig_stream_convert_channels()` | Mono ↔ stereo                     |
| `rig_stream_resample()`         | Sample rate conversion (F32)     |
