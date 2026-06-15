# TCI 2.0 Backend for Expert Electronics SunSDR Radios

New Hamlib backend (RIG_MODEL_TCI2, dummy family model 12) for TCI
(Transceiver Control Interface) protocol version 2.0.  Tested against
SunSDR2 Pro running ExpertSDR3 v1.1.7 on Arch Linux.

TCI is a WebSocket push protocol, so this backend lives in rigs/dummy/.  The
backend does an HTTP/1.1 upgrade handshake over Hamlib's existing TCP
layer (no external WebSocket library needed), drains the server's READY
dump on connect, and keeps a local state cache updated by every incoming
push.  Reads come from the cache or issue explicit queries; writes send
the appropriate TCI command.

## What Works (CAT)

- **Frequency**: VFO A and VFO B (set/get), split TX frequency
- **Mode**: all modes from the server's MODULATIONS_LIST
  (AM, LSB, USB, CW, NFM, WFM, RTTY, DIGL, DIGU on SunSDR2 Pro)
- **PTT**: RIG_PTT_ON, RIG_PTT_ON_MIC, RIG_PTT_ON_DATA (TCI source:
  default, Mic, tci)
- **Split** and **VFO switching**
- **RIT and XIT** (enable/offset)
- **Levels**: AF volume, RF drive, squelch, NB threshold, AGC
  (off/fast/normal), CW keyer speed, S-meter, SWR, TX power
- **Functions**: NB, NR, ANF, mute, tune (ATU)
- **CW**: send_morse (CW_MACROS with TCI escape encoding), stop_morse,
  wait_morse
- **Config params**: trx (TRX index, default 0), txsource, digl_offset,
  digu_offset

Default path: 127.0.0.1:50001

## Why Audio Support in a Hamlib Backend?

Normally the audio path for a digital-mode radio sits entirely outside
Hamlib: the soundcard is a separate device, the modem (JS8Call,
fldigi, WSJT-X, ...) opens it directly, and Hamlib only does CAT.
TCI doesn't fit that model.  Two relevant facts:

1. Besides the legacy Kenwood emulation, TCI is the only documented
   protocol ExpertSDR3 exposes for external software, and it carries
   CAT *and* audio over the same WebSocket.  There's no separate
   audio interface to plug a modem into.

2. ExpertSDR3 will only stream audio to the single TCI client that
   last asserted PTT.  You cannot have rigctld own the TCI CAT
   connection and a separate process own the TCI audio connection:
   there is no such thing; the radio refuses to send audio to a
   client that didn't assert PTT. This is a deficiency in the TCI
   protocol specification, but it is what it is, and it's too late
   to change the standard now. Perhaps TCI v3.0 will support
   separating CAT and audio.

So if Hamlib owns the TCI connection (which it must, for CAT to work),
the audio also has to flow through Hamlib.  Anything else means giving
up either CAT-via-Hamlib or audio-to-modems entirely.  Neither is an
acceptable answer for a backend whose users are running JS8Call etc.

The minimal-invasiveness choice: rigctld continues to be a CAT-only
process from its own point of view, and the audio is shipped as
opaque bytes over a small documented TCP sidechannel to whatever
external process wants to handle the modem-side audio plumbing.
The sidechannel is only opened when explicitly configured.  When the
config param is absent (the default), the backend behaves exactly
like a normal CAT-only Hamlib driver -- no listen socket, no audio
thread, nothing to review beyond standard backend territory.

## How the Audio Sidechannel Works

Enabled by `-C audio_port=N` on the rigctld command line (or the
equivalent set_conf call from a Hamlib-using application).  When
enabled, the backend:

- Opens a TCP listen socket on 127.0.0.1:N (loopback only).
- Accepts one connection at a time from a sidecar process.
- Spawns a single reader thread that owns all WebSocket reads from
  the TCI connection for the lifetime of the open.
- Forwards inbound binary TCI audio frames (RX_AUDIO_STREAM) to the
  sidecar verbatim, framed with TCI's own 64-byte header so the
  sidecar doesn't have to know about WebSocket framing.
- Forwards a TX_CHRONO marker line to the sidecar whenever the
  server requests TX audio.
- Reads complete TCI binary frames coming back from the sidecar
  (reassembled across recv() calls -- TCP is a byte stream and a
  naive single-recv approach silently corrupts long frames, which
  ExpertSDR3 then drops silently) and forwards them to the TCI
  server as TX_AUDIO_STREAM.
- Auto-sets txsource to "tci" when audio_port is enabled, so PTT
  asserts with the TCI audio source already selected.

Inbound TCI text frames remain handled by the backend itself;
nothing changes about CAT.  The reader thread queues text frames and
the CAT thread pops from the queue, which is what makes a single
WebSocket reader work without races between CAT request/reply and
audio.

Total added code in the backend for the audio sidechannel is on the
order of a few hundred lines, all gated by `priv->audio_port > 0`.
The protocol over the sidechannel TCP socket is documented:

- **Framing**: each message is a 64-byte TCI header (8 little-endian
  uint32 fields, the rest reserved zero) followed by a body whose
  length is determined by the header's `length`, `format`, and
  `channels` fields.  This is the same wire format TCI uses on the
  WebSocket, minus the WebSocket framing.
- **Direction** is identified by the header's stream_type field
  (1 = RX_AUDIO_STREAM, 2 = TX_AUDIO_STREAM, 3 = TX_CHRONO).
- **Sample rate, sample type, and channel count** are negotiated by
  the sidecar issuing AUDIO_SAMPLERATE / AUDIO_STREAM_SAMPLE_TYPE /
  AUDIO_STREAM_CHANNELS as text TCI commands during init.

This means a sidecar can be written in any language, runs as a
separate process under the user's account (no special privileges,
no kernel components), and can be killed/restarted without
disturbing rigctld or the radio.

Note that the current version does not handle IQ streams. That will be
in an upcoming release, but didn't seem mandatory for a v1.0. Like
with the audio stream, the TCI protocol mandates that IQ data be
streamed to the CAT controller, it cannot be streamed to a different
TCI client. Which means the Hamlib driver will have to proxy IQ the
same way it proxies audio. This feature is planned for a future update.

## The Reference Sidecar

A reference Linux sidecar is maintained at
https://github.com/jfrancis42/hamlib-tci-sidecar.  It creates two
PulseAudio null sinks (`tci-rx` and `tci-tx`) using `pactl`, pumps
RX audio into the rx sink with `pacat`, captures TX audio from the
tx sink's monitor with `parec`, and bridges both directions to
rigctld over the audio_port socket.  Modems point at the standard
PulseAudio device names and never know they're talking to a
SunSDR2.  This works on any current desktop Linux that has either
PulseAudio or PipeWire with the pipewire-pulse shim, which is
essentially all of them.

Windows and macOS sidecar versions are planned and will use the
same TCP sidechannel protocol.  On Windows the audio glue would
target VB-Cable via WASAPI (`sounddevice`), on macOS BlackHole via
CoreAudio (also `sounddevice`).  The protocol code is identical
across platforms; only the audio plumbing differs.

If audio_port is disabled (the default), none of this is reachable
from any code path: no thread, no socket, no allocations.

## Protocol Quirks Worth Knowing

- The server sends everything lowercase; the backend uppercases the
  keyword on receive so sscanf format strings work.
- Mode strings are built entirely from the server's MODULATIONS_LIST,
  so only modes the server supports are ever sent.  Requesting a mode
  the server doesn't know returns -RIG_EINVAL.
- set_mode does NOT write RX_FILTER_BAND: ExpertSDR3 auto-applies a
  sane filter on MODULATION change, and empirical testing shows that
  sending RX_FILTER_BAND right after MODULATION corrupts the server's
  filter state.  get_mode reports the server's actual filter from the
  READY-dump cache.
- VFO_LOCK (TCI 2.0) is server-to-client only; set_func LOCK returns
  -RIG_ENAVAIL.
- CW_MACROS_SPEED and CW_KEYER_SPEED are separate; get_level KEYSPD
  returns the macro speed from the READY cache.
- ExpertSDR3 silently drops TX audio frames whose payload level is
  below some internal threshold -- TX engages but the PA puts out
  zero watts, with no error response.  The sidecar handles this with
  a configurable post-amplifier on its TX path; the backend itself
  just forwards bytes and does not modify levels.
- `tci2_send` is internally serialised so the reader thread (which
  issues AUDIO_START etc when a sidecar connects) and the CAT thread
  (which issues normal Hamlib commands) can both write to the
  WebSocket without colliding.

## Verification

Verified end-to-end on a SunSDR2 Pro / ExpertSDR3 v1.1.7 / Arch Linux:
- CAT via rigctld: frequency / mode / split / PTT cycle, with
  cross-checks against ExpertSDR3's GUI.
- Audio sidechannel: JS8Call running unmodified against the
  sidecar-created PulseAudio devices, RX waterfall live and TX
  keying actual RF on a spectrum analyzer at the expected dial+1500
  Hz audio offset, RFPOWER_METER tracking accordingly.
