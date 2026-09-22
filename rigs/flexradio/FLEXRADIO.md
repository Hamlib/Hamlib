# FlexRadio backends

**FlexRadio Systems** is the manufacturer. **SmartSDR** is the software that
runs on its current radios and the TCP/IP API they are controlled through, so
the backend that speaks it is named for the API rather than for a model: one
backend serves the whole FLEX-6000 and FLEX-8000 line. This file is named for
the directory, and the directory holds three unrelated FlexRadio backends of
different vintages:

| Backend | Files | What it talks to |
|---|---|---|
| **SmartSDR** | `smartsdr*.c/h` | FLEX-6000 and FLEX-8000 radios, over the SmartSDR TCP/IP API and VITA-49 UDP |
| DttSP | `dttsp.c` | DttSP SDR software on the host, not a radio |
| SDR-1000 | `sdr1k.c` | The FlexRadio SDR-1000, over a parallel port |

Only SmartSDR is current hardware, and the rest of this document covers
**SmartSDR** alone. The other two are legacy and unrelated to it: they share
the manufacturer and this directory, nothing else.

---

## 1. Models

SmartSDR presents up to eight independent receivers ("slices", lettered A–H).

Use model **23013** and name the slice with the `slice` setting:

```sh
rigctl -m 23013 -r 192.168.0.252:4992            # slice A, the default
rigctl -m 23013 -r 192.168.0.252:4992 -C slice=B # slice B
```

Models **23005–23012** each bind to one slice (23005 = A through 23012 = H).
They predate the `slice` setting and keep working unchanged, so existing
configurations need no edits; 23013 is the one to use for anything new. A
per-slice model behaves exactly as 23013 with the matching `slice` setting.

Slices are created and destroyed at runtime on the radio. If the slice is not
there when you open, `slice_missing` decides what happens:

- **`CREATE`** (default) — make one, and remove it again on close. The radio
  chooses the index of a new slice; it cannot be asked for a particular one.
  So this can satisfy a rig that did not name a slice, and it can satisfy a
  named one only when that slice happens to be the next free. If you asked for
  D and the radio offers B, the backend removes B again and reports
  unavailable rather than quietly controlling the wrong receiver.
- **`FAIL`** — `rig_open` returns unavailable and nothing is created.

A slice the backend created starts at 14.100 MHz USB on ANT1 and is expected
to be tuned immediately; a frequency has to be supplied at creation, and
nothing is known about your setup at that point.

## 2. Connecting

SmartSDR speaks a line-based text protocol over **TCP port 4992**:

```sh
rigctl -m 23013 -r 192.168.0.252:4992
```

The radio is addressed by IPv4 literal. The control connection would accept a
hostname, but the VITA stream sockets are built from the same string and parse
it as a dotted quad, so a name resolves for the commands and then fails to
carry audio. Shorthand forms such as `192.168.252` are not accepted either.

The default path is `127.0.0.1:4992`, so `-r` is required for a real radio.

**LAN only.** Reaching a radio over FlexRadio's SmartLink service requires
OAuth authentication and encryption, which this backend does not implement. A
radio may report `remote_on_enabled=1` because SmartLink is *configured* on it;
that says nothing about your connection and does not change how Hamlib
connects.

## 3. Configuration

| Token | Type | Default | Meaning |
|---|---|---|---|
| `slice` | A–H | A | Which slice this rig controls, for model 23013. |
| `slice_missing` | CREATE/FAIL | CREATE | What to do when that slice is not on the radio. See §1. |
| `spectrum` | 0/1 | 0 | Deliver panadapter FFT as Hamlib spectrum lines. See §8. |
| `nat_traversal` | 0/1 | 0 | Ask the radio to learn the client's NAT-translated UDP address (`client udp_register`) instead of being told a local port (`client udpport`). Needed **only** when reaching the radio through manual port forwarding. Leave 0 on a LAN. |
| `auto_reconnect` | 0/1 | 0 | Rebuild the control session after the radio stops answering. See §10. |
| `split_slice` | A–H | *(empty)* | Treat this slice as the transmit slice for split instead of whichever slice the radio has marked with `tx=1`. Empty follows the radio. |
| `status_timeout` | ms | 0 | Treat a value the radio reported as stale after this long, so reads report it unavailable. 0 keeps values indefinitely, which suits a radio that pushes status changes. |
| `liveness_timeout` | ms, 500–3600000 | 20000 | Silence from the radio for this long declares the session lost. See §10. |
| `vita_port` | 1–65535 | 4991 | UDP port the radio receives transmit data on, and the port the client prefers for its own socket. Every radio uses 4991; change it only where something else on the host holds that port. |
| `tx_audio_source` | MIC/ACC/PC/DAX | MIC | What the radio modulates from, spelled as the radio names its own inputs. The radio holds the input selection and the DAX flag separately and they can contradict each other, so both are written together. Opening a transmit stream selects `DAX` for as long as it is open, because streamed audio is discarded otherwise. |

```sh
rigctl -m 23005 -r 192.168.0.252:4992 -C nat_traversal=1
```

With `nat_traversal` on, the backend keeps the mapping alive by sending a
1-byte UDP datagram every 30 s — a TCP command cannot refresh a UDP NAT
mapping — plus a `client udp_register` every 5 minutes as a backstop.

`HAMLIB_SMARTSDR_WAN=1` in the environment is an alias for the same token.

## 4. What is implemented

**Control:** frequency, mode and passband, PTT, CW keying (`send_morse` /
`stop_morse`), antenna selection, and a broad set of levels and functions.
Every level carries a range and step, so an application can size a control or
bound a value without guessing — `rigctl -m 23013 -u` lists them.

**Modes** map to the radio's own names: `USB`/`LSB`/`CW`/`AM`/`SAM`/`RTTY`
directly, `PKTUSB`/`PKTLSB` to `DIGU`/`DIGL`, `FMN` to `FMN` (`NFM` is also
accepted on the way in), and `PKTFM` — which Hamlib prints as `FM-D` — to the
radio's `DFM`, its FM for data, which carries its own pre/de-emphasis setting
rather than the voice network.

**No parms.** A parm is a rig-wide setting with no VFO, and the ones Hamlib
defines — `BEEP`, `BACKLIGHT`, `KEYLIGHT`, `SCREENSAVER`, `TIME`, `ANN` —
describe a front panel this radio does not have in that sense; a SmartSDR
radio is driven by clients and its API has none of them. The rig-wide
settings that do matter are chosen once when connecting rather than adjusted
while running, so they are configuration tokens (Section 3), not parms.

Levels map onto the radio's slice and transmit objects:

| Hamlib | SmartSDR |
|---|---|
| `AF`, `SQL`, `RF`, `BALANCE` | `audio_level`, `squelch_level`, `rfgain`, `audio_pan` |
| `AGC` | `agc_mode` (off/slow/med/fast) |
| `NR`, `NB`, `APF` | `nr_level`, `nb_level`, `apf_level` |
| `RFPOWER`, `MICGAIN`, `COMP` | `rfpower`, `mic_level`, `speech_processor_level` |
| `VOXGAIN`, `VOXDELAY` | `vox_level`, `vox_delay` |
| `KEYSPD`, `CWPITCH`, `BKIN_DLYMS` | `speed`, `pitch`, `break_in_delay` |
| `MONITOR_GAIN` | `mon_gain_cw` in CW, `mon_gain_sb` otherwise |

Functions: `MUTE`, `SQL`, `NR`, `NB`, `ANL` (wideband blanker), `ANF`, `APF`,
`LOCK`, `RIT`, `XIT`, `DIVERSITY`, `VOX`, `COMP`, `FBKIN`, `MON`, `TUNER`.

Antennas map `RIG_ANT_1..4` to `ANT1`, `ANT2`, `RX_A`, `XVTA`. `set_ant` moves
the receive antenna always and the transmit antenna only when the radio lists
the port in `tx_ant_list`, so a receive-only port can never become the
transmit antenna.

**Streaming:** DAX audio RX/TX and DAX-IQ RX/TX, as VITA-49 over UDP. See §6.

**Split** — see §5.

**Metering:** `STRENGTH`, `SWR`, `ALC`, `RFPOWER_METER`,
`RFPOWER_METER_WATTS`, `TEMP_METER`, `VD_METER` and `ID_METER`, read from the
radio's meter stream. See §7.

**Spectrum:** panadapter FFT as Hamlib spectrum lines, opt-in. See §8.

**Not implemented:** memory channels, scanning.

## 5. Split, VFOs and slices

SmartSDR has **no second VFO**. A slice is a complete receiver with exactly one
`RF_frequency`; what other radios call VFO B, FlexRadio expresses as *another
slice*. Exactly one slice at a time carries `tx=1`, and setting it on one slice
clears it on every other — the radio does that itself.

So **split means the transmit slice is not the one you are receiving on**, and
the radio's own SPLIT button works by creating a second slice and moving `tx`
to it.

Hamlib still needs VFOs to address. The backend presents **Main** and **Sub**
rather than A/B, because slices are independent receivers and because the radio
already uses the letters A–H for slices — under A/B naming, "VFO A" on a rig
set to `slice=C` would mean Slice C. **A and B are still accepted as
aliases.**

| Hamlib | SmartSDR |
|---|---|
| Main (or A) | this model's slice |
| Sub (or B) | the transmit slice |
| `set_vfo` | `slice set <n> active=1` |
| `set_split_vfo` ON | create a slice, set `tx=1` on it |
| `set_split_vfo` OFF | `tx=1` back on this slice, remove the created slice |
| `set_split_freq` / `set_split_mode` | tune / set the transmit slice |
| `RIG_OP_CPY` | copy this slice's frequency to the transmit slice |
| `RIG_OP_TOGGLE` | exchange the two slices' frequencies |

The Sub VFO resolves to `split_slice` if set, else the slice carrying `tx=1`,
else a slice created on demand — **only when writing**. Reading a Sub VFO that
does not exist reports unavailable rather than creating a slice as a side
effect.

```sh
rigctl -m 23005 -r 192.168.0.252:4992
  S 1 Sub         # split on: creates the transmit slice
  I 14250000      # transmit frequency
  X LSB 2400      # transmit mode
  f               # receive frequency, unchanged
  G CPY           # copy receive frequency to the transmit slice
  S 0 Main        # split off: removes the slice it created
```

The backend **only removes a slice it created itself**. Slices belong to the
radio and may be in use by the operator or another client, so one made in the
SmartSDR GUI is never deleted. If another program moves the transmit slice
somewhere unexpected, `split_slice` pins it.

For small transmit offsets there is also `RIG_FUNC_XIT` with `xit_freq`, which
shifts transmit within a single slice and needs no second slice at all.

**Passband** is reported as the span between the radio's filter edges. Those
edges are relative to the carrier and both are negative for lower-sideband
modes, so `filter_lo=100 filter_hi=2800` is a 2700 Hz passband.

Setting a mode resets that slice's filters to the mode's default, so
`set_split_mode` sends the width immediately afterwards to put it back. A width
given in the same call therefore does survive. Verified on the radio: with
split set to LSB 2400, a second client reading that slice directly sees
`LSB 2400`.

**Caveat:** only this model's own slice has its properties tracked from the
radio's status messages. The transmit slice's mode and width are reported from
what the backend last wrote, so if the operator changes them in the SmartSDR
GUI, `get_split_mode` will not notice.

## 6. Streaming

The table below is what the **hardware** carries: VITA-49 puts float32 audio
and complex float I/Q on the wire, and nothing else. Applications are not
limited to it — the frontend derives a wider set from these and converts
between the two, so a request for 16-bit samples, a mono stream, or a rate the
radio does not run at is served by conversion rather than refused. What is
declared here is the truth about the radio, which is what
`--require-native` and the conversion reporting are measured against.

| Type | Format | Rates (Hz) | Channels |
|---|---|---|---|
| `AUDIO_RX` / `AUDIO_TX` | `PCM_F32` | 24000 | 2 |
| `IQ_RX` | `IQ_CF32` | 24000, 48000, 96000, 192000 | 1 |

`rigstreamtest` prints which stages are carrying a stream, and
`--require-native` refuses one that would need any:

```
Stream AUDIO_RX: conversions=0x0 (native stream)
Stream AUDIO_RX: conversions=0x5 (converted stream)   # format + channels
```

Rate conversion needs libsamplerate at build time; format and channel
conversion are always available. Without it, the rates above are the only ones
a stream can open at.

```sh
rigstreamtest -m 23013 -r 192.168.0.252:4992 -t audio_rx -d 10 -w out.wav
rigstreamtest -m 23013 -r 192.168.0.252:4992 -t iq_rx -s 48000 -c 1 -d 10
```

I/Q needs `-c 1` — I and Q are components of one complex sample, not two
channels. The backend binds the panadapter DAX-IQ needs when the stream opens,
so nothing has to be prepared first.

Both TX types advertise `RIG_STREAM_CAP_TIMED_TX_COARSE` and
`RIG_STREAM_CAP_BURST_PTT` with a 30 s scheduling horizon: the radio gates
play-out at a requested start time and keys PTT around a burst, but does not
schedule individual samples in hardware.

The radio references DAX-IQ to the **panadapter centre**, not the slice
frequency — they differ whenever the slice is not centred in its panadapter.

### Stream timestamps

Both RX types advertise `RIG_STREAM_CAP_HW_TIME`: the backend can carry the
radio's own time when the radio has a clock worth carrying. What you actually
get is reported per read, in `rig_stream_read_info`:

| radio | `time_source` | `time_accuracy` |
|---|---|---|
| GPSDO fitted, and the stream carries UTC/GPS real-time stamps | `SRC_GPS` | `ACC_100NS` |
| TCXO only, and the radio's stamp advances | `SRC_RADIO` | `ACC_US` |
| otherwise | `SRC_HOST` | `ACC_MS` |

`SRC_RADIO` means the timeline comes from the radio and is stable, but its
epoch is not traceable to UTC — a TCXO disciplines rate, not absolute time.
The backend decides per stream by watching whether the radio's fractional
timestamp actually advances, so on a radio that stamps one stream and not
another, each is reported for what it is. On a FLEX-8400M, DAX-IQ reports
`SRC_RADIO` and DAX audio reports `SRC_HOST`, because the audio stream repeats
the same fractional value in every packet.

`rig_stream_get_hardware_time()` asks the same question at a point in time
rather than per read, and answers it from the same three rows: the latest
GPS-traceable stamp the radio sent, or the host clock labelled `SRC_RADIO` or
`SRC_HOST` according to what the radio has. A TX stream receives no packets to
stamp, so it answers from the host clock and says so.

### The transmit monitor cannot be heard over the network

`RIG_FUNC_MON` and `RIG_LEVEL_MONITOR_GAIN` do reach the radio: they set
`mon` and `mon_gain_sb`, and reading them back reflects the radio's state.
The audio, however, never arrives anywhere Hamlib can deliver it. SmartSDR
does not mix the monitor into DAX, which FlexRadio state plainly — *"MON is
not available with DAX at this time"* — and it is on their list rather than
in the product.

Measured on a FLEX-8400M running SmartSDR 4.2.20, which still behaves this
way: while keyed, the DAX receive stream is exact digital silence, and the
`remote_audio` stream carries three-byte Opus frames, which is silence too.

So a monitor is the application's own business. Anything transmitting through
`rig_stream_write()` already holds the samples it is sending and can play them
locally; there is nothing to fetch from the radio. The monitor is audible at
the radio's own headphone or speaker output, and works normally when
transmitting from the microphone or USB input instead of DAX.

**There is no I/Q transmit.** DAX I/Q is a receive path: it binds to a
panadapter, and the radio offers no transmit stream type for it. Measured on a
FLEX-8400M, streaming a half-scale tone to a `dax_iq` stream with PTT keyed
produces exactly the same 0.001 W as streaming silence, so nothing is being
modulated from it. `stream create type=dax_iq ... tx=1` and
`type=dax_iq_tx` are both accepted and both come back as a plain `dax_iq`
stream with `endpoint_type=Not Assigned`. Transmit from a client is
`type=dax_tx`, the audio path, which `AUDIO_TX` uses.

**Byte order is not uniform.** Audio, meter and FFT payloads are big-endian
network order; DAX I/Q is little-endian, and its floats carry counts against a
full scale of 32768 rather than the ±1.0 the streaming API promises. The
backend handles both. The radio also reports `payload_endian=little` in the
`dax_iq` stream status, which is worth knowing if you are reading the protocol
directly.

## 7. Metering

The radio streams meter values over UDP rather than answering a query, so the
backend subscribes on the first meter read and every later read returns the
last value the radio sent. A session that never reads a meter never subscribes.

| Hamlib level | Meter | Reported as |
|---|---|---|
| `STRENGTH` | `LEVEL` | dB relative to S9 |
| `SWR` | `SWR` | ratio |
| `ALC` | `ALC` | fraction of full scale |
| `RFPOWER_METER` | `FWDPWR` | fraction of rated power |
| `RFPOWER_METER_WATTS` | `FWDPWR` | watts |
| `TEMP_METER` | `PATEMP` | °C |
| `VD_METER` | `+13.8B` | volts |
| `ID_METER` | `PACURRENT` | amperes |

The radio names its meters and assigns each an index when a client subscribes.
Those indices are **not** fixed: they differ between radios, between firmware
versions, and even between sessions depending on whether the rig was receiving
or transmitting when the subscription was made. The backend therefore matches
meters by name and follows renumbering as the radio reports it.

Supply voltage, PA current and PA temperature are sent only as they change
rather than on a timer, so the *first* read of one of those can take a second
or two. Subsequent reads are immediate.

**Limitation:** if the very first meter read happens while the rig is already
transmitting, `ALC` reports unavailable, because that numbering omits it. Open
the rig and read a meter while receiving — the ordinary case — and it works
across subsequent transmit cycles.

## 8. Spectrum

With `spectrum=1` the backend reads the panadapter belonging to this rig's
slice and delivers each FFT frame as a Hamlib spectrum line:

```sh
rigctl -m 23013 -r 192.168.0.252:4992 -C spectrum=1
```

An application receives lines through `rig_set_spectrum_callback()`. **The core
rejects that call until the rig is open**, so register the callback after
`rig_open()`, not before.

The slice must already have a panadapter — the one the operator sees. Opening
with `spectrum=1` on a slice that has none reports unavailable rather than
making one, because a panadapter that no slice feeds produces a single frame
and then a flat line, which looks exactly like a working spectrum of a dead
band.

The backend asks the radio to widen that panadapter to 512 bins so a frame
carries useful detail; a client that owns it may refuse, which costs resolution
and nothing else. Frames are reassembled when the radio splits one across
datagrams.

Bin values are heights within the panadapter, running from 0 at the top — the
strongest signal, at `max_dbm` — down to the panadapter's height at `min_dbm`.
Hamlib reports larger as stronger, so the backend inverts them and reports the
radio's own `min_dbm`/`max_dbm` as the line's signal strength range.

## 9. How it works

```
  Radio                          Backend                         Application
  ─────                          ───────                         ───────────
    │  TCP 4992                     │                                 │
    │◄──── C<seq>|command ─────────►│  smartsdr_session.c             │
    │      R<seq>|status|body       │  transactions, status parsing   │
    │      S<handle>|object …       │                                 │
    │                               │                                 │
    │  UDP VITA-49                  │  smartsdr_stream.c              │
    │◄─────── samples ─────────────►│  one socket, dispatch by ID ───►│ ring buffers
```

- **`smartsdr.c`** — backend registration, the `rig_caps` blocks for the ten
  models, and the rig lifecycle: init, open, close and cleanup.
- **`smartsdr_priv.h`** — the private data every module shares, the
  enumerations that index it and the configuration token numbers.
- **`smartsdr_conf.c`** — the settings a rig accepts and the handlers that
  read them into the private data.
- **`smartsdr_session.c`** — the `C`/`R`/`S` transaction layer: the thread
  that reads the control connection, TCP line reassembly, the status parser,
  and the keepalive and reconnect threads.
- **`smartsdr_props.c`** — the tracked radio state: which status key carries
  each property, how a status line updates it, and how a write sets it.
- **`smartsdr_slice.c`** — which slice this rig drives and which one
  transmits, and so split, VFO selection and the VFO operations.
- **`smartsdr_level.c`** — levels, functions and antenna, driven by a table
  mapping each Hamlib setting to a slice or transmit property.
- **`smartsdr_rig.c`** — frequency, mode, PTT, RIT, XIT, tuning step and the
  keyer, plus the conversion between Hamlib modes and SmartSDR's names.
- **`smartsdr_stream.c`** — the shared UDP socket and its dispatcher, the
  per-stream threads, and the meter and panadapter readings.
- **`smartsdr_vita.c`** — the VITA-49 codec: bytes in, values out, touching
  no socket and no rig.

The radio sends every VITA datagram to the one UDP endpoint a client
registered, so the socket belongs to the rig rather than to a stream. A single
dispatcher thread reads it and hands each packet to the stream whose
`stream_id` matches, which is what lets an audio and an I/Q stream run at the
same time. Each TX stream keeps its own thread, since transmitting is a
producer rather than a consumer, and sends on that same socket.

The control connection has a reader of its own, for the same reason the UDP
socket does: everything on it arrives unasked. That thread owns the read side,
absorbs `S` lines the moment they come and hands each `R` line to the caller
waiting for the sequence number its command carried, so two callers can have
commands outstanding without taking each other's answers. Callers write for
themselves. What this buys is that tracked state is current whether or not the
application is issuing commands: a program that opens the rig and only streams
still sees the radio change as it changes.

Status arrives as unsolicited `S` lines at any time, and the radio never
answers a "read this property" request — the only refresh is re-subscribing,
which disturbs audio. The backend therefore records each property as it is
reported, in a table indexed by property rather than by retaining the status
text, and a successful write records the accepted value too (the radio does
not echo level or function changes back).

Only lines for the rig's own slice update its state. `interlock` and
`transmit` lines are radio-wide and always apply, and every slice's
frequency and `tx` flag is tracked so the transmit slice can be found for
split.

## 10. Troubleshooting

**Commands appear to succeed but nothing changes.** Check the slice exists
(`in_use=1`) and that the model matches the slice you mean. A model bound to a
non-existent slice still opens.

**No audio from a stream that reports success.** Confirm `nat_traversal` is 0
on a LAN. With it wrongly enabled the radio is never told the client's UDP
port and sends nothing, while the stream still opens successfully.

**I/Q stream fails to open.** The caps declare one channel, so ask for one:
`rigstreamtest -m 23013 -r HOST:4992 -t iq_rx -c 1`. If it opens but no data
arrives, the panadapter binding is what to look at: the backend uses the
slice's own panadapter and creates one when the slice has none, so the log
shows either the pan it found or a `display pan create`. See §6.

**The radio drops the connection after ~15 s.** SmartSDR disconnects clients
that stop pinging. The backend runs a keepalive thread; if it cannot start,
`rig_open` now fails rather than returning a session that is about to die.

**Losing the radio.** Anything the radio sends is proof it is still there, so
the backend watches for silence rather than for a command that failed: nothing
at all for `liveness_timeout`, 20 s by default, marks the session lost, as does
a write that fails outright. The keepalive pings at half that interval, capped
at 10 s because the radio drops a client that has been quiet for about fifteen,
so a link that is merely slow still proves itself. Raise the token on a link
that stalls for seconds at a time:

```sh
rigctl -m 23005 -r 192.168.0.252:4992 -C liveness_timeout=45000
```

Loss sets `comm_status` to `RIG_COMM_STATUS_DISCONNECTED` with a reason of
`LINK_TIMEOUT` or `SOCKET_ERROR`. Open streams are failed rather than left to
stall, so a reader gets `-RIG_EIO` immediately instead of blocking forever on a
ring buffer nothing will ever fill again.

With `auto_reconnect=1` the backend then reopens the socket and re-registers,
retrying with a backoff that doubles from 1 s and holds at 30 s:

```sh
rigctl -m 23005 -r 192.168.0.252:4992 -C auto_reconnect=1
```

**Streams are not restored.** Their stream IDs died with the session and the
radio has no memory of them, so an application must reopen any stream it had
open — which is exactly why they are failed rather than silently stalled.
Control commands work again as soon as the session is back.

**`client program` reports `0x10000002`.** The radio's program whitelist does
not list Hamlib. This is not fatal and is treated as success.

## 11. Testing

```sh
make -C test check          # includes test_smartsdr_stream and test_vita49
```

`simulators/simflex` is a TCP/UDP simulator covering the command protocol and
VITA-49 traffic; `test_smartsdr_stream` drives the backend against it.
`SIMFLEX_REMOTE_ON=1` makes it model a SmartLink-enabled radio.

For hardware, `tests/rigstreamtest-hw.sh` runs every streaming mode the model
advertises, records what it received for listening, and prints a pass/fail line
per mode. It needs a radio, so it is not part of `make check`:

```sh
tests/rigstreamtest-hw.sh -m 23013 -r 192.168.0.252:4992
tests/rigstreamtest-hw.sh -m 23013 -r 192.168.0.252:4992 \
    --tx --freq 14100000 --mode USB --power 0.01     # into a dummy load
```

Sample rates come from what each stream advertises, so audio runs at 24 kHz and
I/Q at 48 kHz without being told. `rigstreamtest --list-streams` prints that
table for any model.

`tests/rigstreamtest-dummy.sh` is the simulator-backed counterpart and does run
under `make check`.

## 12. Reference

SmartSDR TCP/IP API:
<https://github.com/flexradio/smartsdr-api-docs/wiki/SmartSDR-TCPIP-API>
