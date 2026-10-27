# AnyTone AT-778UV / Retevis RT95 Hamlib Backend

A different radio and a different protocol from the AT-D578UVIII in this same
directory, sharing only the manufacturer. Models **37002** (AnyTone AT-778UV)
and **37003** (Retevis RT95).

## The same radio under other badges

This is one Qixiang design sold under at least five names:

| badge | declared here | notes |
|---|---|---|
| **AnyTone AT-778UV** | **37002** | developed and tested on this one |
| **Retevis RT95** | **37003** | `RIG_STATUS_UNTESTED` - see below |
| CRT Micron UV | no | same design, nobody has tested one |
| Midland DBR2500 | no | same design, nobody has tested one |
| Yedro YC-M04VUS | no | same design, nobody has tested one |

The `-P` suffixed variants (RT95-P, AT-778UV-P) are the same radios with VOX
fitted.

The RT95 is declared despite being untested because the protocol was worked
out **from its firmware**: the RT95 LCD image and the RT95 RF processor image
were the primary references for the command dispatcher, and the AT-778UV was
what happened to be on the bench. So the evidence for it is unusually strong
for something marked untested - but it is still untested, and it says so.

The three undeclared badges are left out only because nobody has run one.
Adding them is a two-line change to `riglist.h` plus a caps struct; reports
from owners are welcome.

**The backend does not check which model answers.** Each badge reports its own
identify string - an RT95 answers `RT95` where an AT-778UV answers `AT778UV` -
so rejecting a mismatch would refuse exactly the radios this is meant to
drive. The reported string is logged at verbose level instead, and is the
useful thing to quote in a bug report.

## Connection

Control is over **pin 8 of the RJ45 microphone socket** — a single wire, half
duplex, 9600 8N1, shared by the hand microphone's keypad and by the
programming cable. The usual programming cable works unmodified.

```bash
rigctl -m 37002 -r /dev/ttyUSB0
```

**There is no PTT over this bus.** The microphone socket carries PTT on a
discrete pin (pin 2, pulled to ground), so key the radio through that pin or
through a serial control line:

```bash
rigctl -m 37002 -r /dev/ttyUSB0 --set-conf=ptt_type=RTS,ptt_pathname=/dev/ttyUSB1
```

### Clients that poll get_ptt unconditionally

Because there is no CAT PTT, `.ptt_type` is `RIG_PTT_NONE` and `get_ptt`
returns `-RIG_ENAVAIL`. Some client software polls `t` (get_ptt) every cycle
without checking `dump_caps` first, and treats the failure as fatal - so the
dashboard stays empty even though the connection is fine and every other
command works.

Two honest ways round it, neither needing a change here:

```sh
# Best: give Hamlib a real PTT line to read. It reports the actual state.
rigctld -m 37002 -r /dev/ttyUSB0 -P RTS -p /dev/ttyUSB1

# Or: let Hamlib answer from its own cache of what it last commanded.
rigctld -m 37002 -r /dev/ttyUSB0 -P RIG
```

With `-P RIG` and no backend callback, `rig_get_ptt` returns the frontend's
cached `transmit` flag, so `t` answers `0` and the client's poll cycle
proceeds. Note what that value means: it is what Hamlib last **commanded**,
not what the radio is doing. Key the rig through its hardware PTT pin and it
will still report receive. Do not rely on it for carrier sense or TX
interlocks.

The backend deliberately does not implement `get_ptt` itself. It cannot: the
RF processor knows the transmit state but never forwards it to this bus, so
any value it returned would be a guess dressed up as a reading.

## What works

`set_freq` / `get_freq`, `set_vfo` / `get_vfo`, `get_dcd`, `set_mode` /
`get_mode` (FM 25 kHz, FM 20 kHz, FMN 12.5 kHz), AF / SQL / RFPOWER levels,
`RIG_FUNC_MUTE`, CTCSS and DCS encode and decode, and repeater offset.

## Memory mode is refused, deliberately

The radio's parameter-set command writes whichever record is currently
selected — and in memory mode that is the **stored memory channel**, not the
VFO. The write is acknowledged, the display does not change, and the channel
is overwritten permanently. A client retuning in that state would walk through
the operator's memory channels destroying them one at a time, with no visible
symptom.

So every record write reads the VFO/memory state first and returns
`-RIG_ERJCTED` if the selected slot is on a memory channel. Switch the radio
to VFO mode and retry.

Note the error code is deliberately **not** `-RIG_ENTARGET`, which would be
the obvious choice: `rig_set_freq` converts that one to `RIG_OK` on purpose
(*"we will just return RIG_OK and the frequency set will be ignored"*), which
would report success for a write that was refused.

The backend does not switch the radio out of memory mode for you. It cannot do
so reliably: the radio has no VFO/MEM key. VFO/MEM is a *function code* that
one of the four programmable microphone keys may be bound to, and on a radio
where none is bound there is no route to it at all.

## Reading the radio back

Every getter reads live state, so a client that polls sees the operator's own
changes: turning the dial, the volume knob, or the squelch menu all show up
within one poll. Verified on the radio.

**The radio never announces anything.** It emits a status frame when the
squelch opens or the monitor key is pressed, but frequency and channel changes
produce no traffic at all, so there is nothing to push and `.transceive` is
`RIG_TRN_OFF`. Hamlib's poll routine only diffs its own cache and publishes
change events; it never reads the rig. So change notifications arrive only as
fast as the client polls, and the 1 s cache timeout sets the floor.

**Getters follow the radio into memory mode.** The record address is resolved
from the live block before every read: the VFO record when the slot is in VFO
mode, the memory channel's record otherwise. Without that, `get_freq` reports
the VFO's frequency while the radio is listening on a memory channel - which
it did, silently, until it was measured. `get_mem` reports the channel number,
and is the only way a client can see the operator move into memory mode before
a write gets refused.

### Squelch 0 pins get_dcd on

`RIG_LEVEL_SQL` maps onto the radio's 0-9, where **0 is squelch OFF**. That is
a legitimate setting and it applies live - but with the squelch open the radio
asserts carrier detect permanently:

```
L SQL 0        ->  0x3204 = 00,  get_dcd = 1, 1, 1   (no signal present)
L SQL 0.222    ->  0x3204 = 02,  get_dcd = 0
```

COS on this radio means "the audio gate is open", not "a signal is present",
and nothing distinguishes the two. So anything that polls `get_dcd` for
carrier sense will see a permanently busy channel while the squelch is open.
Measured on the radio; the backend cannot detect or work around it, because it
is reporting exactly what the radio reports.

**This does not affect Direwolf**, and by extension most packet software.
Direwolf uses Hamlib for PTT only - it refuses anything else outright,
*"HAMLIB can only be used for PTT.  Not DCD or other output."* - and derives
carrier detect from its own demodulator, listening for the data carrier in the
audio. Running with the squelch open is in fact the usual advice for 1200-baud
AFSK, because hardware squelch is too slow and clips the start of each burst.

So the warning applies to software that actually polls `get_dcd`, not to a
packet stack doing its own audio-domain detection.

### Unsolicited status frames are discarded, deliberately

The radio emits a status frame whenever the squelch opens or the monitor key
is pressed. When one arrives in the middle of an unrelated exchange the
transport consumes it, logs it, and carries on - it is not our reply.

It is tempting to turn those into `rig_fire_dcd_event()` calls instead. Don't,
without thinking it through:

- **`rig_fire_dcd_event` has no callers anywhere in Hamlib.** Firing it would
  be inventing a convention, not following one.
- The only precedent for firing events at all is `icom_process_async_frame`,
  and that runs on the **frontend's** reader thread, not inside a backend
  transaction. Calling a user callback from the middle of a synchronous
  request/reply exchange risks re-entrancy.
- It would catch only the frames that happen to land during a transaction,
  since the rest are flushed - a partial, unpredictable event stream.

The supported route is the async framework (`.async_data_supported`,
`.is_async_frame`, `.process_async_frame`), used by seven backends, where the
frontend owns the read path. That would give push-based DCD, which matters for
packet where carrier-sense latency affects channel access - but it means
reworking a transport on a half-duplex bus that already loses about 2% of
exchanges to collisions, for one capability. Worth doing only if polled DCD
proves too slow in practice.

## Other limitations, all of them the radio's

- **No S-meter.** The RF processor measures signal strength and the display
  draws a bar graph from it, but the value is never sent to the microphone
  bus. `RIG_LEVEL_STRENGTH` is not offered.
- **No `get_ptt`.** Transmit state exists in the radio's internal status
  object and is likewise never forwarded.
- **No `set_rptr_shift`.** Writing an offset also forces the record's duplex
  field to a negative shift, and nothing in the command set writes that field
  with any other value. An offset can be set but never cleared.
- **`get_func(RIG_FUNC_MUTE)`** is not offered. Mute is a bit in a RAM byte
  that the status frame masks off before sending, so there is no honest read.
- **VOX** is fitted only to the `-P` variants, which are identified by the
  model string ending in `P`. Untested.
- **Dual watch** is readable but cannot be changed without rebooting the
  radio, so it is not exposed. It is logged at open, because with dual watch
  on a carrier can appear on either slot — which is why `get_dcd` reports the
  logical OR of both.

## Protocol notes

Three commands do the work: `0x44` writes a live parameter, `0x52` reads a
16-byte codeplug block, and `0x41` injects a microphone keypress.

- **Block reads must be exactly 16 bytes.** A longer read returns a
  well-formed, correctly checksummed frame whose tail is stale buffer content.
- **The parameter frame is rigidly 7 bytes.** The trailing `0x06` is
  positional, not a delimiter; a short frame leaves the radio waiting and
  swallowing whatever is sent next.
- **Everything is echoed** before the reply arrives, and both ends can talk at
  once, so replies are validated rather than merely counted.
- **Stray ASCII is dangerous.** The dispatcher matches on the first byte: a
  frame beginning `E` reboots the radio, `R` swallows the next three bytes as
  an address, and `F` enters a factory test mode that only removing DC power
  clears.

Protocol analysis from the CHIRP `anytone778uv` driver, from reverse
engineering the AT-778UV and RT95 LCD firmware images and the RT95 RF
processor image, and from bench measurement on an AT-778UV running V200.
