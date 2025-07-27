# HAMLIB 5.0 - Release Notes (DRAFT)

This major release includes a restructuring of HAMLIB internal storage, forcing
changes to the Application Binary Interface(ABI).

## New Hardware
- TBD

## Build changes
- No more K&R C - C compiler needs at least c11 capability.
- Functions rig_get_conf(), rot_get_conf() and amp_get_conf() have been removed.
  See issue #924.
- shortfreq_t changed to int?

## Application changes
