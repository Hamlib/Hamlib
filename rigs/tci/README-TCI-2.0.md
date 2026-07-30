# TCI 2.0 backend (CAT only)

**Backend model:** 12 (TCI 2.0)
**Protocol:** TCI 2.0 over WebSocket
**Target hardware:** Expert Electronics ExpertSDR3 (SunSDR2 family), Apache
Labs ANAN with TCI firmware, and any other radio that speaks TCI 2.0.

> **Scope.** This backend implements **CAT only** — frequency, mode,
> PTT, split, RIT/XIT, levels, functions, CW keying, etc.  It does
> **not** carry audio or IQ.  The TCI protocol does define both
> directions of audio and an RX IQ stream, and a follow-up PR will add
> that support; for now, binary WebSocket frames carrying audio or IQ
> are silently discarded by this backend.  CAT alone is useful with
> radios on the same machine as the digital-mode app, where the app
> can use the radio's own audio device, or with separate audio
> plumbing (PulseAudio loopbacks, ALSA modules, JACK, network audio,
> etc.) outside of Hamlib.

## Quick start

```bash
rigctld -m 12 -r 10.1.1.52:50001 -t 4532 -vv
```

Then point any Hamlib client at `localhost:4532` (or wherever rigctld
is listening).  JS8Call / WSJT-X / fldigi / log4om / Hamlib-aware
loggers — anything that speaks `Hamlib NET rigctl` — will work.

The `-r` argument is the TCI server's WebSocket endpoint:
`host:port` (no `ws://` prefix).  Default ExpertSDR3 listens on
`localhost:50001` if the radio software is on the same machine.

## Configuration parameters

Set via `-C key=value` on the rigctld command line.

| Token | Type | Default | Range / values | Meaning |
|-------|------|---------|----------------|---------|
| `trx` | int | 0 | 0..7 | Which TRX to control on multi-receiver hardware (e.g. SunSDR2 DX has two simultaneous transceivers). |
| `txsource` | string | `default` | `default`, `mic`, `vac` | Audio source the radio will use when keyed.  This is a TCI protocol field that travels in `TRX:trx,true,<source>;`.  No audio bytes touch this backend; it just tells the *radio* which input feeds its TX modulator.  Overridden per call by `RIG_PTT_ON_MIC` (forces `Mic`) and `RIG_PTT_ON_DATA` (forces TCI audio source). |
| `digl_offset` | int | 0 | 0..4000 (Hz) | Frequency offset for the radio's DIGL (digital-LSB) mode.  Sent to the server on connect. |
| `digu_offset` | int | 0 | 0..4000 (Hz) | Frequency offset for the radio's DIGU (digital-USB) mode.  Sent to the server on connect. |

Example with all params:

```bash
rigctld -m 12 -r 10.1.1.52:50001 -t 4532 \
        -C trx=0 -C txsource=vac \
        -C digl_offset=1500 -C digu_offset=1500
```

## What's implemented

| Hamlib operation | TCI command(s) used | Notes |
|---|---|---|
| `set_freq` / `get_freq` | `DDS:` + `VFO:` (set), `VFO:` (get) | DDS first then VFO — see "Gotchas" below. |
| `set_mode` / `get_mode` | `MODULATION:` | Width comes from the server's auto-applied filter (see "Gotchas"). |
| `set_vfo` / `get_vfo` | client-side cache only | TCI has no "current VFO" concept; we track A/B locally. |
| `set_ptt` / `get_ptt` | `TRX:trx,true,<source>;` / `TRX:trx,false;` | `<source>` chosen from `txsource` config + per-call `RIG_PTT_ON_*`. |
| `set_split_vfo` / `get_split_vfo` | `SPLIT_ENABLE:` | TX VFO is always B when split is on. |
| `set_split_freq` / `get_split_freq` | `VFO:trx,1,freq;` | Direct write to VFO B. |
| `set_rit` / `get_rit` | `RIT_OFFSET:` + `RIT_ENABLE:` | Setting 0 turns RIT off. |
| `set_xit` / `get_xit` | `XIT_OFFSET:` + `XIT_ENABLE:` | Setting 0 turns XIT off. |
| `set_level` / `get_level` (`AF`, `RFPOWER`, `SQL`, `AGC`, `NB`, `KEYSPD`, `STRENGTH`, `SWR`, `RFPOWER_METER`) | various TCI levels | S-meter, SWR, and TX power-out are read from the cached `RX_SENSORS` / `TX_SENSORS` stream. |
| `set_func` / `get_func` (`NB`, `NR`, `ANF`, `MUTE`, `TUNER`, `LOCK`) | `RX_NB_ENABLE:` / `RX_NR_ENABLE:` / `RX_ANF_ENABLE:` / `MUTE:` / `TUNE:` / read-only `VFO_LOCK:` | `LOCK` is server-push-only in TCI 2.0; we expose the cached value. |
| `send_morse` / `stop_morse` / `wait_morse` | `CW_MACROS:` / `CW_MACROS_STOP;` | Two-phase wait: up to 2 s for TX to engage, then up to 5 min for TX to release. |
| `power2mW` / `mW2power` | client-side | Assumes 100 W max; the actual maximum is hardware-dependent. |
| `get_info` | client-side | Returns `"TCI 2.0 — <device> (trx <n>)"`. |

## TCI-protocol architecture

```
   ┌──────────────────────────┐    TCI 2.0 (WebSocket)    ┌──────────────────────┐
   │  ExpertSDR3 / SunSDR /   │◀─────────────────────────▶│  rigctld -m 12       │
   │  ANAN-with-TCI server    │   single connection,      │  (this backend)      │
   │  (default :50001)        │   text + binary frames    │                      │
   └──────────────────────────┘                           └─────────┬────────────┘
                                                                    │
                                                                    │ NET rigctl
                                                                    │ (default :4532)
                                                                    ▼
                                                  JS8Call / WSJT-X / fldigi /
                                                  Log4OM / any Hamlib-aware
                                                  digital-mode or logging app
```

TCI is push-based: when the rigctld backend connects, the server streams
its full current state (frequency, mode, filter, sensors, etc.) and
finishes with `READY;`.  After that, any state change — by us or by
another connected client (e.g.  the radio's own GUI) — is broadcast
to every client.  This backend maintains a state cache in
`struct tci2_priv` that's updated by every incoming text frame, and
returns cached values for several read-only sensors that the server
streams periodically rather than answering on query.

Binary WebSocket frames (audio and IQ) are silently consumed and
discarded.

## Gotchas (control-only)

These are findings from on-air bring-up that aren't obvious from the TCI
spec.  Anyone extending this backend or porting it to a different host
language should know them.

### `DDS:` before `VFO:` on `set_freq`

TCI distinguishes two related-but-not-identical concepts:

- **DDS** is the panorama center / IQ-stream local oscillator.  It's
  where the receiver actually tunes.
- **VFO** is the cursor inside the panorama — where the receiver
  demodulates relative to DDS.

A bare `VFO:trx,ch,freq;` only moves the cursor.  The IQ stream's LO
stays put, the panorama doesn't scroll, and an IQ-mode consumer keeps
demodulating whatever was at the original DDS frequency — the
frequency change is silent.  This was reproducible on the SunSDR2 PRO.

So `tci2_set_freq` sends DDS first, then VFO:

```c
SNPRINTF(cmd, sizeof(cmd), "DDS:%d,%.0f;", priv->trx_num, freq);
tci2_send(rig, cmd);
SNPRINTF(cmd, sizeof(cmd), "VFO:%d,%d,%.0f;", priv->trx_num, ch, freq);
tci2_send(rig, cmd);
```

The reverse order (VFO then DDS) was also tested — it works, but the
panorama snaps visibly.  DDS-first is smoother.

### `set_mode` does not set the filter

ExpertSDR3 auto-applies a mode-appropriate filter on every
`MODULATION:` change.  Sending an explicit `RX_FILTER_BAND:` afterward
produces wrong filter state in the server — empirically observed,
cause unclear.

Therefore `tci2_set_mode` sends only `MODULATION:` and ignores its
`width` argument.  `tci2_get_mode` returns the width from the cached
`RX_FILTER_BAND` values that the server pushed during the READY dump
(and continues to push on every filter change).

### Stale-echo drain before each command

Every TCI text command we send produces an echo from the server, and
the server also pushes telemetry (RX_SENSORS at 200 ms, etc.).  In a
CAT-only configuration without a continuous reader thread, these
frames accumulate in the kernel socket buffer between command-response
cycles.  Without a drain, the next `tci2_recv_until` matches its
prefix on the *previous command's* echo and returns one cycle stale.
This was visible on AGC round-trips: set OFF/read → OFF; set FAST/read
→ OFF (stale).

Solution: `tci2_drain()` is called at the head of every `tci2_send`.
It's a non-blocking `poll()`-and-consume loop that drains whatever's
already in the socket buffer, updating the state cache for each frame
seen.  Cost is negligible (most calls find zero frames).

### `RX_FILTER_BAND:N;` query is not supported

A bare `RX_FILTER_BAND:0;` is interpreted by the server as a SET
command with no arguments — and returns garbage internal values.  The
backend never queries it; the cached value from the READY dump (and
subsequent unsolicited pushes) is authoritative.

### Modulation strings vary across server versions / firmware

The static `tci2_mode_map[]` is the canonical map from TCI strings to
Hamlib `rmode_t`, but the server's `MODULATIONS_LIST` (sent during
READY) is the source of truth for what *this particular* radio
accepts.  `tci2_build_mode_list()` parses the list at connect time and
builds a runtime table that's consulted first for outgoing strings
(`MODULATIONS_LIST` says `NFM` but the static map's primary for FMN is
`FMN`?  Send `NFM`).  Falls back to the static map only if the server
didn't send a list.

### `LOCK` is server-push, not query-response

In TCI 2.0, `VFO_LOCK:trx,ch,bool;` is unsolicited — sent when the
lock state changes — but there's no `LOCK:trx;` query that returns
the current state.  `get_func RIG_FUNC_LOCK` returns the cached value
from the READY dump and any subsequent `VFO_LOCK:` pushes.
`set_func RIG_FUNC_LOCK` returns `-RIG_ENAVAIL` because TCI doesn't
define a client→server lock command.

### `CW_MACROS_SPEED` is push-only

Similar story for the CW keyer speed: the server sends
`CW_MACROS_SPEED:wpm;` during READY and on any speed change, but
doesn't answer queries.  `get_level RIG_LEVEL_KEYSPD` returns the
cached value.  `set_level RIG_LEVEL_KEYSPD` works (via the
`CW_KEYER_SPEED:wpm;` set command) — it updates the cache and is
visible in subsequent reads even though the server itself doesn't
re-echo the value.

### Multi-receiver TRX selection

On hardware that exposes multiple TCI transceivers (SunSDR2 DX has
two), the `trx` config parameter chooses which one this rigctld
instance controls.  Run two `rigctld` instances on different ports to
control both:

```bash
rigctld -m 12 -r host:50001 -t 4532 -C trx=0   # first transceiver
rigctld -m 12 -r host:50001 -t 4533 -C trx=1   # second transceiver
```

Each gets its own state cache; one rigctld won't see frequency changes
the other makes (until the server broadcasts them, which it does —
the cache updates from any `VFO:` push regardless of who initiated
it).

## Verified on-air

Tested against an Expert Electronics SunSDR2 PRO running ExpertSDR3 at
14.078 MHz / PKTUSB, exercising:

- Frequency (5 round-trips across 80m / 40m / 30m / 20m / 15m / 10m)
- Mode (8 modes: CW, USB, LSB, AM, FMN, RTTY, PKTUSB, PKTLSB)
- Split on/off and split TX-freq
- RIT and XIT (positive, negative, zero)
- AGC (OFF, FAST, SLOW)
- AF, SQL, KEYSPD levels
- NB, NR, ANF, MUTE functions
- VFO A/B selection
- STRENGTH, SWR, and TX-power-meter reads (cached from sensor streams)

All 53 round-trips passed; baseline state was preserved exactly across
the test.  See `PR_DESCRIPTION.md` for the broader picture.
