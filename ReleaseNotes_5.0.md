# HAMLIB 5.0 - Release Notes (DRAFT)                           2026-12-01(target)

This major release includes a restructuring of HAMLIB internal storage, forcing
changes to the Application Binary Interface(ABI).

## New Hardware
- TBD

## Build changes
- No more K&R C - C compiler needs at least c11 capability.
- Functions rig_get_conf(), rot_get_conf() and amp_get_conf() have been removed.
  See [issue #924](https://github.com/Hamlib/Hamlib/issues/924).
- shortfreq_t changed to int32?

## Application changes
Many of the internal HAMLIB data structures have been moved out of the rig_struct
area into their own heap buffers, and are addressed by pointers. Any application that
wishes to access these structure will have to add a new `#include` statement and
use a macro defined in that `.h` file to obtain the address.


| Structure | Old reference | Include file | New Pointer |
| --------- | ---------------- | ----------------- | ------------ |
| Rig state | `rig->state` | `<hamlib/rig_state.h>` | `HAMLIB_STATE(rig)` |
| Rig CAT port | `rig->state.rigport` | `<hamlib/port.h>` | `HAMLIB_RIGPORT(rig)` |
| Rig PTT port | `rig->state.pttport` | `<hamlib/port.h>` | `HAMLIB_PTTPORT(rig)` |
| Rig DCD port | `rig->state.dcdport` | `<hamlib/port.h>` | `HAMLIB_DCDPORT(rig)` |
| Amplifier state | `amp->state` | `<hamlib/amp_state.h>` | `HAMLIB_AMPSTATE(amp)` |
| Amplifier port | `amp->state.ampport` | `<hamlib/port.h>` | `HAMLIB_AMPPORT(amp)` |
| Rotator state | `rot->state` | `<hamlib/rot_state.h>` | `HAMLIB_ROTSTATE(rot)` |
| Rotator port | `rot->state.rotport` | `<hamlib/port.h>` | `HAMLIB_ROTPORT(rot)` |
| Rotator port2 | `rot->state.rotport2` | `<hamlib/port.h>` | `HAMLIB_ROTPORT2(rot)` |
