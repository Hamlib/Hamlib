# AnyTone AT-D578UVIII Hamlib Backend

Driver for the AnyTone AT-D578UVIII (and AT-D578UV Pro) dual-band mobile radio.

## Dual-mode operation

A single rig model (37001) supports two serial protocols via the `-C commode=` runtime flag:

**Default (`commode=0`)** — direct serial mic protocol:
- PTT on/off (VARA, fldigi, etc.)
- No handshake, no radio display lockout
- Keys PTT on whichever VFO is selected — behaves like the physical mic

**COM mode (`commode=1`)** — BT-01 ADATA protocol:
- PTT, get/set frequency, get/set VFO, set clock
- Radio displays "EXTERNAL CABLE MODE" while connected

```bash
# PTT only (no lockout)
rigctl -m 37001 -s 115200 -r /dev/ttyUSB0

# Full control (locks display)
rigctl -m 37001 -C commode=1 -s 115200 -r /dev/ttyUSB0
```

### COM mode frequency limitations

- **set_freq** requires Channel A selected with VFO A in VFO mode (not MR/memory mode)
- **VFO B selected**: get_freq returns VFO A's frequency; set_freq is refused
- **PTT** works regardless of VFO/mode selection

## Raw command passthrough

In COM mode, `rigctld`'s `w` (send_cmd) interface can send arbitrary ADATA commands. The keepalive thread yields to raw commands automatically, so third-party software can use protocol features the backend doesn't yet implement natively.

## Protocol notes

The radio exposes two protocols on 115200 8N1:

- **Direct mic**: raw 0x06 keepalive, 0x41 PTT. No init, no lockout.
- **BT-01 ADATA**: `+ADATA:00,NNN\r\n` framing. Requires COM MODE handshake.

Protocol analysis based on [jrobertfisher/AT-D578UV-software-mic](https://github.com/jrobertfisher/AT-D578UV-software-mic) and firmware reverse engineering of the D578UV v1.21 and BT-01 v1.02 firmware images.
