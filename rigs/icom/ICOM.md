# Icom backends

Icom radios all speak **CI-V**, one command protocol, but they reach it over
three different transports. Everything in this directory is that one protocol
plus the transport and per-model differences around it.

| Transport | How CI-V travels | Notes |
|---|---|---|
| **Serial** | CI-V over RS-232, usually through a CI-V level converter | The original transport. One bus, addressable radios. |
| **USB** | CI-V over the radio's built-in USB-serial port | Same framing as serial. The radio echoes commands back, which the backend has to account for. |
| **Network** | CI-V tunnelled inside Icom's RS-BA1/LAN UDP protocol | Adds audio and I/Q streams alongside control. The subject of most of this document. |

The CI-V command layer (`icom.c`, `frame.c`) is shared by all three. Only the
transport underneath it differs, so a model's capabilities, level mappings and
quirks live in its own file regardless of how it is connected.

---

## 1. Network backends

The network models are clones of their serial counterparts with the LAN
transport and streaming wired in. Capabilities, levels and modes are identical
to the serial model; only the transport, the model name and the streaming
support differ.

| Model | Radio | Streams | State |
|---|---|---|---|
| 3095 | IC-7610 (Network) | AUDIO_RX, AUDIO_TX, IQ_RX | Beta — hardware tested |
| 3096 | IC-9700 (Network) | AUDIO_RX, AUDIO_TX, IQ_RX | Beta — hardware tested |
| 3097 | IC-705 (Network) | AUDIO_RX, AUDIO_TX, IQ_RX | Beta — hardware tested |
| 3098 | IC-905 (Network) | AUDIO_RX, AUDIO_TX, IQ_RX | Untested over the network |
| 3099 | IC-7760 (Network) | AUDIO_RX, AUDIO_TX, IQ_RX | Untested over the network |
| 3100 | IC-7300MK2 (Network) | AUDIO_RX, AUDIO_TX, IQ_RX | Untested over the network |

"Untested" means the model descriptor was written from the shared
implementation but no one has run it against that radio. It is expected to work;
it has not been shown to.

## 2. Connecting

The host is the radio's own address, and the credentials are the **network
user** configured in the radio's menu — not a Hamlib account.

```sh
rigctl -m 3095 -r 192.168.1.50 -C net_username=USER,net_password=PASS
```

Both are limited to **16 characters**, which is all the protocol's obfuscated
credential field can carry. Anything longer is refused when the token is set,
rather than silently truncated into a login failure that looks like a wrong
password.

Streaming uses the same options:

```sh
rigstreamtest -m 3095 -r 192.168.1.50 \
    -C net_username=USER,net_password=PASS -t audio_rx -d 10 -w capture.wav
```

`tests/rigstreamtest-hw.sh` drives every advertised stream mode in one go and
records each one; see `HAMLIB_STREAMING.md` §12 for worked examples.

## 3. Configuration

All tokens are set with `-C name=value`. `rigctl -m MODEL -L` lists them with
their ranges at runtime.

| Token | Meaning | Values | Default |
|---|---|---|---|
| `net_username` | Radio's network user | ≤ 16 characters | — |
| `net_password` | That user's password | ≤ 16 characters | — |
| `net_control_port` | Control port | 0–65535, 0 = 50001 | 0 |
| `net_radio_index` | Which advertised radio to use | −1 = select by name, else 0–7 | −1 |
| `net_radio_name` | Radio to select by name | empty = this model's own name | empty |
| `net_iq_mode` | Present stereo RX as I/Q | 0 / 1 | 0 |
| `net_rx_codec` | RX wire codec | **index** into the list below | 0 |
| `net_tx_codec` | TX wire codec | **index** into the list below | 0 |
| `net_sample_rate` | Wire sample rate | **index**: 0=48000, 1=24000, 2=16000, 3=12000, 4=8000 | 0 |
| `net_rx_latency` | RX reorder window (see *Packet loss* below) | 0–1000 ms, 0 = none | 0 |
| `net_tx_latency` | TX jitter buffer | 10–1000 ms | 150 |
| `net_tx_enable` | Reserve a TX audio path | 0 / 1 | 1 |
| `net_tx_frame_ms` | TX wire frame length | 5–100 ms | 20 |
| `net_liveness_timeout` | Silence before the session is declared lost | 0 = never, else ≥ 1000 ms | 5000 |
| `net_auto_reconnect` | Re-establish a lost session in the background | 0 / 1 | 0 |

> **The codec and rate tokens take an index, not a value.** `net_sample_rate=4`
> means 8000 Hz, not 4 Hz. `net_sample_rate=48000` is an index far outside the
> list and is **rejected** (`-RIG_EINVAL`) rather than quietly substituting a
> default — running at a rate you did not ask for, without being told, is the
> worse failure. This trips people up; the combo list in `rigctl -L` is the
> reference.

RX codec indices: `0` LPCM 1ch 16bit, `1` LPCM 1ch 8bit, `2` µLaw 1ch 8bit,
`3` LPCM 2ch 16bit, `4` µLaw 2ch 8bit, `5` LPCM 2ch 8bit, `6` ADPCM 1ch.
TX codec indices: `0` LPCM 1ch 16bit, `1` LPCM 1ch 8bit, `2` µLaw 1ch 8bit,
`3` ADPCM 1ch. **ADPCM is rejected** — the codec is defined by the protocol but
not implemented here.

### Stereo receive

A dual-receiver radio can carry both receivers in one stream, one per channel.
Two independent things are needed:

- `-C net_rx_codec=3` — the two-channel **wire** codec. Chosen when the session
  opens, so it must be config, not something inferred later from the stream.
- a two-channel **application** stream (`rigstreamtest -c 2`).

Asking for two channels without the wire codec gives a two-channel file with
the same mono audio in both. Enable the second receiver as well:

```sh
rigctl -m 3096 -r ADDR -C ... --vfo
  U MainA DUAL_WATCH 1     # second receiver on
  L SubA SQL 0             # or it sends digital silence
```

Two model notes learned the hard way:

- On an IC-9700, `MainA` and `SubA` address the two receivers independently;
  plain `Main`/`Sub` and `VFOA`/`VFOB` both land on the primary receiver.
- A closed squelch makes a receiver send **exact digital silence**, which in a
  recording is indistinguishable from a channel that has stopped working.

### What the radio carries, and what Hamlib converts

The session settles on one sample rate and one codec per direction when it
connects, and those decide what this backend can put on the wire. Once the rig
is open its advertised streaming capabilities say exactly that: the negotiated
rate, and the channel count of the negotiated codec — one entry for receive,
one for transmit, since the two are configured separately.

**The backend hands the payload over without copying or converting it whenever
the wire already carries what the stream carries.** That covers every linear
PCM codec: the 16-bit ones are `PCM_S16` on the wire, the 8-bit ones are
`PCM_U8`, and either goes to the application exactly as it came off the
network. Only the companded and block codecs are genuinely decoded.

So each session serves **one** native format, and which one depends on the
codec it negotiated:

| `net_rx_codec` | wire bytes/sample | native format | backend does |
|---|---|---|---|
| `0` LPCM 1ch 16bit, `3` LPCM 2ch 16bit | 2 | `PCM_S16` | nothing |
| `1` LPCM 1ch 8bit, `5` LPCM 2ch 8bit | 1 | `PCM_U8` | nothing |
| `2` µLaw 1ch 8bit, `4` µLaw 2ch 8bit | 1 | `PCM_S16` | decodes µLaw |

The 8-bit LPCM codecs are the reason to pick one: half the traffic of the
16-bit codecs, and no work at either end. µLaw is one byte on the wire too and
saves exactly as much bandwidth, but it is companded rather than linear and
Hamlib has no µLaw sample format, so it always reaches the application through
the 16-bit pivot. It is still worth choosing for a constrained link — the
decode is cheap and local, and the saving is on the network.

Anything other than the session's native format is reached by the frontend
converting, which is reported rather than hidden: `native_formats` is the one
format the session hands over untouched, and `formats` is everything openable
once those conversions are counted. Asking an 8-bit session for `PCM_S16` works
and reports a format conversion; asking it for `PCM_U8` reports none.

Note that "native" says the frontend added nothing, not that no work happened:
a µLaw session serves `PCM_S16` natively even though the backend decoded every
sample to produce it.

To see it on a radio, ask for the format and demand it arrive unconverted
(`rigstreamtest` reports the stream as native when no conversion stage runs):

```sh
# 8-bit stereo codec: U8 is native, and carries half the bytes of S16
rigstreamtest -m 3095 -r ADDR -C net_username=...,net_password=...,net_rx_codec=5 \
    -t audio_rx --format u8 --require-native -s 48000 -c 2 -d 5

# the whole sweep, one case per format the model declares
tests/rigstreamtest-hw.sh -m 3095 -r ADDR -C net_username=...,net_password=... \
    --tests native
```

A format the model declares but *this* session cannot serve natively is
reported as a skip naming the stages that would have converted (FORMAT, RATE,
CHANNELS), not as a failure — the model declaration covers every
configuration the radio has, and one session is only ever a slice of it.
`rigstreamtest` prints that refusal as a line of its own,
`refused: native stream required, needs=<stages>`, which is what the sweep
matches.

Anything else an application asks for is met by the frontend, which converts
between its stream and the radio's. Asking for float samples, or a rate the
session did not negotiate, or a channel count the codec does not carry, all
work; `rigstreamtest` prints which conversion stages are active (for example
FORMAT and CHANNELS for float stereo from a mono 16-bit session), and
`--require-native` refuses a stream that would need any of them.

Rate conversion needs libsamplerate at build time. Without it the negotiated
rate is the only rate a stream can open at, so choose it with
`net_sample_rate` rather than expecting the frontend to bridge.

### Codec and rate combinations that carry no audio

A codec and a rate are configured separately, and not every pairing works. On
an IC-7610 and an IC-9700 the radio accepts the connection, assigns an audio
port and reports no error, then never sends a single audio packet — for exactly
one rate per codec:

| `net_rx_codec` | wire layout | dead `net_sample_rate` |
|---|---|---|
| `0` LPCM 1ch 16bit | 1 ch × 2 bytes | `1` (24000) |
| `1` LPCM 1ch 8bit | 1 ch × 1 byte | `0` (48000) |
| `2` µLaw 1ch 8bit | 1 ch × 1 byte | `0` (48000) |
| `3` LPCM 2ch 16bit | 2 ch × 2 bytes | `3` (12000) |
| `4` µLaw 2ch 8bit | 2 ch × 1 byte | `1` (24000) |
| `5` LPCM 2ch 8bit | 2 ch × 1 byte | `1` (24000) |

Every other pairing of those six codecs with the five rates carries audio
normally — 48 of 60 measured combinations across the two radios, with the 12
failures falling exactly on the table above.

The pattern is one rule: **the radio sends nothing when the wire data rate is
exactly 48000 bytes per second**, that being `rate × channels × bytes-per-sample`.
Every failing pairing computes to that figure and no working pairing does.

The cause is not established, and it is worth being precise about what the
radio does and does not claim here. It publishes a sample-rate capability
bitmap, and every rate above is in it — an IC-7610 reports `0x8B01`, which is
12k, 48k, 16k, 8k and 24k — so the session layer's rate check passes, because
the radio really does advertise the rate. It publishes **nothing at all about
codecs**: the capability entry has no codec field, so codec support cannot be
queried and is only ever discovered by trying. Nothing in this backend is keyed
to a byte rate; the connection request carries a codec and a rate and no more.
That leaves the radio's own packetiser as the only place the quantity is
computed, which is why this reads as firmware behaviour rather than something
Hamlib can negotiate around.

If a stream opens cleanly and reports zero bytes with no gaps, no overruns and
no errors, check the product against 48000 before looking anywhere else.

### Two views of the capabilities

The model declaration and the session are different things, and they are
reported separately:

| view | shows | when |
|---|---|---|
| `rigctl -m MODEL --dump-caps` | what this **radio model** can do across every configuration — all five rates, mono and stereo | needs no connection |
| `\stream_caps` (rigctld), `rig_stream_caps_at()` | what **this connection** can carry: the negotiated rate and codec, widened by whatever the frontend can convert | once the rig is open |

`--dump-caps` therefore reports the same thing whether or not a rig is open; a
connected session never rewrites it. To see what you can actually open right
now, ask the open rig.

### I/Q needs the two-channel codec

I/Q is not a separate path in this protocol: it is the receive audio path
carrying `{I,Q}` pairs, so it needs the two slots of a stereo wire codec. A
session negotiated with a mono codec cannot carry it at all, and does not offer
it — `IQ_RX` is simply absent from that session's capabilities:

```sh
# mono session: opening I/Q fails with "no caps found for stream type"
rigstreamtest -m 3095 -r ADDR -C net_username=...,net_password=... -t iq_rx

# stereo codec: I/Q is offered and opens
rigstreamtest -m 3095 -r ADDR -C net_username=...,net_password=...,net_rx_codec=3 \
    -t iq_rx -c 1
```

The two slots are the I and Q components of a **single** complex channel, so an
I/Q stream is one channel (`-c 1`), never two. `rigstreamtest-hw.sh` skips
`iq_rx` with "not offered by this session" rather than failing when the codec
does not allow it.

## 4. How the network transport works

Three UDP sockets, all opened towards the radio's control port (50001) and then
redirected to the ports the radio assigns:

| Socket | Default port | Carries |
|---|---|---|
| Control | 50001 | Handshake, authentication, keepalive, session teardown |
| CI-V | 50002 | CI-V frames, complete with `FE FE` … `FD` |
| Audio | 50003 | Audio and I/Q sample payloads |

The handshake runs in a fixed order, each step a separate stage in
`icom_network_session_connect()`:

```
probe/present -> ready -> login -> token create -> capabilities
    -> open data sockets -> connection info -> status
    -> reconnect data sockets -> CI-V stream open -> keepalive threads
```

Every packet carries a sequence number. On the CI-V and control sockets lost
packets are re-requested and replayed from a small transmit buffer; a loss
burst wider than that buffer can recover causes a resynchronisation instead,
logged and counted. All of the radio's numbered packets are tracked, idles
included: it numbers its idles in the same sequence as its CI-V frames, so
tracking only the frames saw a false gap for every idle. A CI-V frame that
arrives twice is delivered once, since a repeated ACK or reply would be taken as
the answer to the next command. A sender that starts counting again (a reopened
stream) is recognised and tracking restarts from it. Audio and I/Q follow their
own rules, below.

Packets are classified by the socket they arrive on. Lengths alone are
ambiguous: a CI-V or audio packet can be exactly as long as a token or status
packet, so on the CI-V and audio sockets packets are recognised by their own
structure, and the fixed-size management packets only on the control socket.

The handshake is protected the same way. A reply from the radio that is lost
on the way (login, capabilities, connection info) is noticed from the gap in
the radio's sequence numbers and requested again; resending the request would
not help, because the radio has already received it and does not answer it
twice. The radio numbers a stream's tracked packets from 0 (the login response
is 0, the capabilities reply 1), and tracking expects that, so even the very
first reply is recovered when it is lost. A connect that fails part-way hands its token back and disconnects
before returning. Otherwise the radio would keep the half-open session's slot
and refuse the next attempts for tens of seconds.

### Packet loss on audio and I/Q

Received audio and I/Q packets are delivered **in sequence order**, never in
the order they happen to arrive, and every loss is reported to the stream:

- **`net_rx_latency=0` (the default).** Nothing is held back: each packet is
  passed on as it arrives. A missing packet is given up as soon as the next one
  arrives, and anything that turns up later — a reordered packet, a duplicate —
  is dropped rather than played out of order. No retransmits are requested,
  because a resend could only arrive behind audio already delivered.
- **`net_rx_latency=N` (1–1000 ms).** Packets are held up to N ms, counted from
  the packet after a hole, so reordered packets are put back in place and
  missing ones are requested from the radio while they can still arrive in
  time. This adds up to N ms of latency; on a clean LAN a few tens of
  milliseconds is enough for one retransmit.

Whatever is still missing when the window closes is reported, not hidden:

- **Audio** is filled with silence for its measured length, so playback stays
  continuous; the read carries `RIG_STREAM_DROP_GAP | RIG_STREAM_DROP_CONCEALED`
  and `rig_stream_get_stats()` counts `gaps` and `concealed_samples_gap`.
- **I/Q** is left as a hole in the sample index (`info.dropped_samples`,
  `dropped_samples_gap`): invented samples would corrupt the phase a
  demodulator tracks.
- A burst too long to size (a restart of the radio's counter) is reported as an
  unsized gap (`gaps_unknown`).
- If the application reads too slowly and the backend's receive queue
  overflows, the dropped audio is reported as a local overrun (`overruns`,
  `concealed_samples_overrun`) rather than disappearing.
- An ADPCM decoder is reset at every loss, so it does not predict across it.

The radio splits each 20 ms frame into packets of at most 1364 bytes (16-bit
mono is 1364 + 556, 16-bit stereo 1364 + 1364 + 1112), so a lost packet's
length is taken from the last packet seen at the same position in the frame.

**Choosing a window.** Measured on an IC-7610 (16-bit mono at 48 kHz, about
100 packets a second), from a Mac on Wi-Fi (8–37 ms ping to the radio), with 5%
of the packets from the radio dropped deliberately. Each entry is the number of
packets still lost after 20 s, out of about 2000:

| Window | 0 ms | 20 ms | 50 ms | 100 ms | 200 ms |
|---|---|---|---|---|---|
| Packets lost | 96 | 5 | 1 | 1 | 0 |

A resend usually arrives within 20 ms. While the window lasts, the backend asks
again every half window, but never more often than every 20 ms. A 20 ms window
therefore gets a single attempt, which fails when the resend is lost as well. A
50 ms window's second attempt helps only if its resend comes back within the
remaining 25 ms. In practice:
- **0** for the lowest latency: every loss is concealed and counted;
- **20 ms** recovers about 95% of losses with a single attempt each;
- **50 ms or more** adds a second attempt and recovered about 99% in this test.

To measure your own link, use `tests/rigstreamtest-hw-loss.sh` (macOS or Linux,
needs `sudo`). It makes the link lossy on purpose and reports, for each loss rate and
window, how many losses remain and how many retransmits were requested. Pick the
smallest window after which the remaining losses stop falling.

### Code layout

| Layer | Files | Responsibility |
|---|---|---|
| 1 — wire format | `network_proto.[ch]` | Pure encode/decode. No sockets, no state, no threads. |
| 1 — sequencing | `network_seqbuf.[ch]` | Replay buffer and gap tracking. Pure bookkeeping. |
| 2 — session | `network_session.[ch]` | The three sockets, the handshake, keepalive threads, CI-V and audio queues. |
| 3 — backend | `icom_network.[ch]` | Hamlib `rig_caps`: open/close, streams, config, the per-model registry. |
| 3 — models | `ic*net.c` | One descriptor per radio. |
| — config | `network_conf.[ch]` | The `net_*` tokens. |

The CI-V seam is at layer 2: `icom.c` sends and receives CI-V frames exactly as
it does over a serial port, and the session tunnels them. Nothing in the CI-V
command layer knows the transport is UDP.

## 5. Wire-format essentials

Enough to work on `network_proto.c`; not a full protocol specification.

**Capabilities.** The radio answers the token-create request with a count of
radios (big-endian `uint16` at offset `0x40`) followed by fixed **`0x66`-byte**
entries starting at `0x42`; entry *n* is at `0x42 + n * 0x66`. A radio's own
server advertises one; an RS-BA1 PC server can advertise several, which is what
`net_radio_index` and `net_radio_name` select between.

Within an entry: name at `0x10` (32 bytes), audio device name at `0x30`,
connection type at `0x50` (`0x0707` WiFi, `0x073F` Ethernet), CI-V address at
`0x52`, RX rate bitmap at `0x53`, TX rate bitmap at `0x55`, CI-V baud rate at
`0x5A` (big-endian).

**Identity is overloaded.** The first 16 bytes are either a GUID or a MAC-mode
block, discriminated by `commoncap` at offset `0x07`: `0x8010` means the radio
is identified by the MAC at `0x0A`. Since the block is only ever echoed back in
the connection-info request, copying all 16 bytes verbatim is correct in both
modes. Both radios examined here — an IC-7610 and an IC-9700, both on
Ethernet — report MAC mode, so it is not a legacy path.

**Rate bitmaps.** `rxsample`/`txsample` are bitmaps, not values, read
little-endian. In the high byte: `0x80` 12 kHz, `0x40` 44.1 kHz, `0x20`
22.05 kHz, `0x10` 11.025 kHz, `0x08` 48 kHz, `0x04` 32 kHz, `0x02` 16 kHz,
`0x01` 8 kHz; in the low byte, `0x01` is 24 kHz. A radio offering everything it
can reports `0x8B01` — 48/24/16/12/8 kHz, omitting the PC-audio rates. A
`txsample` of `0` means the radio offers **no TX audio**, and the backend then
refuses to open a TX stream.

Note that the rate *negotiated* in the connection-info request is a big-endian
`uint32` in Hz — a different encoding in a different packet from the bitmaps.
The two must not be conflated.

## 6. Adding a network model

Each model is a descriptor and one call. Copy an existing file, e.g.
`ic9700net.c`:

```c
static const struct icom_network_model ic9700net_model =
{
    .base_caps  = &ic9700_caps,          /* the serial model to clone */
    .rig_model  = RIG_MODEL_IC9700NET,
    .macro_name = "RIG_MODEL_IC9700NET",
    .model_name = "IC-9700 (Network)",
    .radio_name = "IC-9700",             /* as the radio's server reports it */
    .rx_only    = 0,
    .iq_capable = 1,
    .status     = RIG_STATUS_UNTESTED,   /* until someone runs it */
    .version    = "20260730.0",
};
```

Then:

1. A model number in `include/hamlib/riglist.h`.
2. `<model>net_init_caps(); rig_register(&<model>net_caps);` in `icom.c`,
   next to the serial model.
3. The file in `rigs/icom/Makefile.am`.
4. Declarations in `icom.h`.
5. The model in `bindings/python/test_Hamlib_class.py`, alphabetically.

`radio_name` is both the default selector and a cross-check: if the radio the
server offers has a different name, the backend warns that the wrong backend
may have been chosen. Getting it wrong makes the open fail with a message
naming what the server actually advertised, so it is self-diagnosing.

If a radio shares a `RIG_IS_*` predicate with model-specific behaviour in
`icom.c`, extend that macro to cover the network model too — the network
variant is the same radio and needs the same quirks.

## 7. Losing the radio

A network session can end without warning: another client takes the radio (only
one may hold it), it loses power, or the network path drops. Only the first of
those makes the radio tell us; the other two are silence.

The backend watches for both, on each socket separately: control, CI-V, and
audio while a stream runs. The radio sends steady traffic on every one of them
(idles, pings and their replies: about 23 packets a second on control, 46 on
CI-V and 115 on audio while streaming, measured on an IC-7610 and an IC-9700),
so if nothing arrives on any one of them for `net_liveness_timeout`, the session
is declared lost. Checking them separately matters: the control socket can go on
exchanging keepalives while the radio has stopped serving CI-V.

Socket errors (the radio's port refusing packets, the network unreachable) do
not end the session by themselves: a path that fails for a moment is ridden out
for as long as `net_liveness_timeout` allows, and `0` still means never. They
name the cause instead: a socket that went silent after reporting an error is
lost as `SOCKET_ERROR` rather than `LINK_TIMEOUT`. While a socket reports
errors, a CI-V command or TX audio packet sent on it fails at once with
`-RIG_EIO`; the next packet received clears that, which on a working CI-V
socket is a few tens of milliseconds away.

An unsolicited disconnect from the radio ends the session immediately. Either
way:

- the loss is logged with its cause;
- `comm_status` becomes `DISCONNECTED` and `comm_reason` says why —
  `PEER_DISCONNECT` (someone else took the radio), `LINK_TIMEOUT` (it went
  quiet) or `SOCKET_ERROR` (a socket kept failing);
- both are published in the multicast snapshot as `status` and `statusReason`
  (multicast is off by default; enable it with `-C multicast_data_addr=224.0.0.1`);
- open streams end: `rig_stream_read()` hands over what was already buffered
  and then returns `-RIG_EIO` rather than blocking — distinguishable from
  `-RIG_ETIMEOUT`, which only means no samples have arrived yet — and
  `rig_stream_write()` returns `-RIG_EIO` at once. `rig_stream_get_stats()`
  reports the same cause in `fail_reason`. A supervisor can treat `-RIG_EIO`
  as "restart the stream (or the process)", and `-RIG_ETIMEOUT` as "idle".

### Reading it from a program

```c
#include <hamlib/rig.h>
#include <hamlib/rig_state.h>

const struct rig_state *rs = HAMLIB_STATE(rig);

if (rs->comm_status != RIG_COMM_STATUS_OK)
{
    fprintf(stderr, "link %s: %s\n", rig_strcommstatus(rs->comm_status),
            rig_strcommreason(rs->comm_reason));
}
```

Use `HAMLIB_STATE()`, not `STATE()` — the latter is internal to the library.

Over the network, a multicast subscriber gets the same two values as `status`
and `statusReason` in the JSON snapshot. **There is currently no way to read
them over the rigctld text protocol**; a `rigctld` client sees only the
per-command errors. Streams do carry it, though: when a stream's source fails,
rigctld sends its client an ERROR frame with the reason, so a netrigctl stream
behind rigctld returns `-RIG_EIO` just like a local one, with the same
`fail_reason`; `\stream_status` shows it as `fail_reason`.

Once the session is lost, control calls keep returning `-RIG_ETIMEOUT` rather
than failing fast. That is deliberate: a false positive must not be able to
break control of a working radio. (The one exception above is a socket that is
reporting errors at that moment, and it clears as soon as anything arrives.)

The threshold must be either 0 (never give up) or at least 1000 ms. The radio's
regular traffic is its answer to a 500 ms keepalive, so a threshold near that
interval would trip on ordinary jitter.

### Automatic reconnection

`net_auto_reconnect=1` re-establishes a lost session in the background, retrying
with a growing delay — 1, 2, 4, 8, 16 seconds, then every 30 — indefinitely. The
early attempts are expected to fail: the radio holds a lost session's slot for
its own timeout before it will accept a new one.

While a reconnect replaces the sockets, CI-V commands and stream calls fail with
`-RIG_EIO` instead of reaching a socket that is being closed. Closing the rig
during a reconnect stops it promptly, even in the middle of a handshake.

Control recovers by itself. **Open streams do not**: the new session has fresh
sequence numbers and possibly different ports, so streams are ended and the
application must reopen them. Reconnection is off by default, because silently
re-establishing a session behind a caller that thinks it still owns one is not a
safe default.

The reported state follows both directions: `comm_status` goes to
`RIG_COMM_STATUS_DISCONNECTED` with a `comm_reason` when the session is lost,
and back to `RIG_COMM_STATUS_OK` once the background retry has it again — see
*Reading it from a program* above for how to read them.

> **How this is tested.** Auto-reconnect is covered by
> `test/test_icom_network_reconnect.c` against the in-process mock radio, which
> can go silent or announce a disconnect on demand. It is **not** exercised on
> real hardware: provoking it needs an unplanned loss, which means physically
> removing the radio. `rigstreamtest -t reconnect` does run against real
> radios, but it closes and reopens the rig deliberately — that covers the
> teardown and re-establish path, not the background retry.

## 8. Troubleshooting

| Symptom | Error | Cause |
|---|---|---|
| Stream read fails mid-session | `-RIG_EIO` | The session was lost; see §7. Check `comm_reason` for which cause. |
| Rejected at login | `-RIG_ESECURITY` | Wrong username or password. The message mentions crypto, but the radio simply refused them. Run with `-vvvv` to see `login rejected`. |
| Nothing answers | `-RIG_ETIMEOUT` | Wrong address, radio's network function off, or the previous session is still held. |
| Opens, then fails | `-RIG_ECONF` | The named radio is not among those advertised, or the configured sample rate is not one the radio offers. The log names what it does offer. |
| Malformed reply | `-RIG_EPROTO` | Capabilities response unusable. |

**A failed connect leaves the radio busy** for its own timeout, so an immediate
retry can fail even with everything correct. Wait ~20 s.

**A silent channel is usually squelch.** A muted receiver sends exact zeros,
which looks identical to a broken stream. Check `SQL` before concluding the
audio path is broken.

**A stream that opens cleanly and then carries nothing** — zero bytes, no gaps,
no overruns, no error — is a different thing from squelch, which delivers zeros
rather than nothing at all. Check the codec and rate against §3: the radio sends
no audio whatsoever when the two multiply out to exactly 48000 bytes per second,
and one such pairing exists for every codec.

`make check` runs the whole handshake, streaming and session-loss paths against
an in-process mock, so none of the suites need a radio. For deliberate
close/reopen cycling against real hardware, use
`rigstreamtest-hw.sh --tests reconnect`.
