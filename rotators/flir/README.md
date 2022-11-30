# FLIR/DirectedPerception PTU Rotor Module

This module interfaces FLIR and DirectedPerception
rotor using the PTU protocol via serial.

This includes:

* PTU-D48(E)
* PTU-E46
* PTU-D100(E)
* PTU-D300(E)

Tested only with PTU-D48 yet and with one rotor per chain only.

## Usage

1. Connect the rotor via serial (RS232 or RS485)
2. Power up the rotor
3. The rotor must be calibrated after each power up. This can be accived
either using the rotctl `Reset` command (R) or manually via serial terminal
sending the `R\n` command.
4. To enable the rotor to fully turn +/- 180°, the softlock must be disabled.
This is included in the rotctl `Reset` commnad or manually via serial terminal
seinden the command `LD\n`. **WARNING:** Send this command only after the rotor is
calibrated, or you risk damage running into the hard endstops (at about +/-190°)
5. Start `rotctl` or `rotctld` with the arguments `-m 2501 -r <Serial
   Interface>`

Have Fun.

### Hints

1. Setup the max. velocity, power and acceleration according to your antenna load.
This must be done via serial terminal, as the functions are not implemented yet.
2. Never use the maximum hold power, only use the low or off. If you use max or regular,
the rotor may easily overheat!

## PTU Protocol

* [Protocol Version 3.02 (2011)](https://flir.netx.net/file/asset/11556/original/attachment)

## Current Status

The current status is **ALPHA**. It is tested with DirectedPercepiton PTU-D48 (Firmware v2.13.4r0(D48-C14/E))
Linux with `rotctl` and `gpredict`.

### Implemented so far:

* init
* cleanup
* open
* close
* set_position
* get_position
* park
* stop
* reset
* move
* info

### Needs to be implemented:

#### Parameters

* velocity
* acceleration
* velocity profile
* user soft-limits
* power commands (move and hold)
* step mode
* reset on startup

#### Functions

* usage of chained rotors via RS485
