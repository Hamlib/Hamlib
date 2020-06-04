# Meade Telescope Rotator Module

This module interfaces Meade telescope rotator via the LX200 serial protocol.

These rotators can easily be modified to carry antennas instead of telescopes.

## Usage

1. Set the telescope manually to show to north with 0 degree elevation
2. Connect the Autostar and the serial cable with the telescope and control PC
3. Turn on telescope rotator
4. Press `Speed` on the Autostar to accept "Don't look into the sun" warning
5. Press `Mode` a couple of times until the Autostar display shows `Object`
6. Start `rotctl` or `rotctld` with the arguments `-m 1801 -s <Serial
   Interface>`

Have Fun.

### Hints

1. The rotator has no lock on 360 degree azimuth.  For example, if you go from
   359 degrees to 002 degrees, it will go the short way.  That means it is
   possible to make several rotations in the same directions.  Please have a
   kind look on your cables and don't use the rotator unattended!
2. If a new position gets sent to the rotor while it is in movement, it moves
   immediately to the new position, but stores the old one and moves again to
   the old one after the new position is reached. To avoid this, a `Stop all
   movement` command gets executed if a position update reaches the rotator
   while in movement.  That can cause a choppy movement when going to a new
   position which is far away.

## LX200 Protocol

* [2003 Protocol Version](https://www.meade.com/support/LX200CommandSet.pdf)
* [2010 Protocol Version](https://www.meade.com/support/TelescopeProtocol_2010-10.pdf)

## Current Status

The current status is **ALPHA**. It is tested with Meade DS-2000 with Autostar
494 (2003 Firmware) and Meade 506 i2c to RS232 interface cable, tested under
Linux with `rotctl` and `gpredict`.

### What works good:

* `set_position` works for azimuth and elevation (0-360,0-90 degree), high
  frequent update (<2s) can cause sometimes unexpected behavior.
* `get_position` works mostly fine, sometime the elevation gets not read
  properly
* `init`, `cleanup`, `get_info` work as expected
* `open` sends all setup commands needed for now
* `close` stops all movement of the rotor
* `meade_stop` stops fine
* `park`, `reset` move the rotor to home position (north, elevation 0)

### What works with some problems:

* `move` works fine for elevation, but for azimuth it will sometimes move in
  the wrong direction caused by the short way movement.
