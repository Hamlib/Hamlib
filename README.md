# AnyTone AT-D578UVIII Hamlib Fork

This is a fork of [Hamlib](https://github.com/Hamlib/Hamlib) with an updated backend driver for the AnyTone AT-D578UVIII (and AT-D578UV Pro) dual-band mobile radio. The upstream Hamlib project provides shared libraries for amateur radio equipment control across hundreds of supported radios — see the [official repository](https://github.com/Hamlib/Hamlib) for full documentation.

This fork focuses on the `rigs/anytone/` backend, adding dual-mode operation, frequency/VFO control, and clock support via the radio's BT-01 Bluetooth microphone serial protocol.

---

## Features

**Default mode** (`commode=0`) — direct serial mic protocol:
- PTT on/off (used by VARA, fldigi, etc.)
- No COM MODE handshake, no radio display lockout
- Keys PTT on whichever VFO is currently selected on the radio, using the properties of that channel or VFO mode — behaves exactly like the physical mic PTT

**COM mode** (`-C commode=1`) — BT-01 ADATA protocol:
- PTT on/off
- Get/set frequency
- Get/set VFO (A/B)
- Set clock (date/time)
- Radio displays "EXTERNAL CABLE MODE" while connected

### COM mode limitations

Frequency control in COM mode has specific requirements:

- **set_freq only works when Channel A is selected AND VFO A is in VFO mode.** If VFO A is in MR (memory) mode, the radio will report the frequency associated with the selected memory channel but will refuse to change it.
- **If VFO B is selected**, get_freq will return the frequency of VFO A (either the VFO entry or the memory channel frequency), not VFO B. set_freq will also be refused in this state.
- **PTT works regardless** — it will key whichever VFO is selected, even VFO B. Even when frequency changes are refused, PTT will still transmit on the currently selected channel.

## Usage

```bash
# PTT only (default, no radio lockout)
rigctl -m 37001 -s 115200 -r /dev/ttyUSB0

# Full control (freq/vfo/clock, locks radio display)
rigctl -m 37001 -C commode=1 -s 115200 -r /dev/ttyUSB0
```

On macOS the serial device is typically `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`.

## Build from source

### Prerequisites

```bash
# Debian/Ubuntu
sudo apt install git build-essential automake autoconf libtool pkg-config libusb-1.0-0-dev

# macOS (Homebrew)
brew install automake autoconf libtool pkg-config libusb
```

### Fresh clone and build

```bash
git clone https://github.com/CowboyPilot/Hamlib.git
cd Hamlib
./bootstrap
./configure --prefix=/usr/local
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
sudo make install
sudo ldconfig 2>/dev/null   # Linux only
```

### Pull updates and rebuild

```bash
cd Hamlib
git pull
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
sudo make install
sudo ldconfig 2>/dev/null   # Linux only
```

If `make` fails after a pull (new files or changed build config), do a full reconfigure:

```bash
./bootstrap
./configure --prefix=/usr/local
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
sudo make install
sudo ldconfig 2>/dev/null   # Linux only
```

## Verify installation

```bash
rigctl -l | grep -i anytone
# Should show:
#  37001  AnyTone  AT-D578UVIII  ...  Beta  RIG_MODEL_ATD578UVIII
```

## Protocol notes

The radio exposes two distinct serial protocols on the same 115200 8N1 port:

- **Direct mic protocol**: raw byte commands (0x06 keepalive, 0x41 PTT). No initialization required, no display lockout. This is what `commode=0` uses.
- **BT-01 ADATA protocol**: `+ADATA:00,NNN\r\n` framed commands. Requires a COM MODE handshake on open and causes "EXTERNAL CABLE MODE" display lockout. This is what `commode=1` enables.

Protocol analysis based on [jrobertfisher/AT-D578UV-software-mic](https://github.com/jrobertfisher/AT-D578UV-software-mic) and firmware reverse engineering of the D578UV v1.21 and BT-01 v1.02 firmware images.

## Raw command passthrough

When `rigctld` is running in COM mode (`commode=1`), third-party software can send arbitrary ADATA commands to the radio using Hamlib's `w` (send_cmd) interface — even for functions the driver doesn't explicitly support. The driver handles COM MODE session management and keepalive automatically; the keepalive thread yields to raw commands so they won't collide.

Example from `rigctl`:

```bash
# Send raw ADATA command 0x57 (1-byte payload) and read 22-byte ACK
w +ADATA:00,001\r\nW\r\n 22
```

From `rigctld` over TCP (port 4532 by default):

```
w +ADATA:00,001\r\nW\r\n 22
```

This lets any ADATA-aware software leverage the driver's session without needing to manage the serial port, COM MODE handshake, or keepalive directly. The radio's firmware supports 34 ADATA command bytes — see the protocol map spreadsheet for the full list.

---

For general Hamlib documentation, API reference, and supported radios, see the [upstream project](https://github.com/Hamlib/Hamlib).
