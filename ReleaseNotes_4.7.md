# HAMLIB 4.7 - Release Notes

The release brings new equipment support, bug fixes/code cleanups, and some
changes to ease transition to 5.0

## New equipment support
- Yaesu FTX-1 Optima/Field - Work in progress, waiting for official
  documentation from Yaesu
- Drake R8/R8A/R8B - new, revised backend
- AF6SA WRC rotator
- GUOHETEC PMR-171 & Q900 moved to separate backend

## Bug Fixes & Code cleanups
- Codebase conforms to ISO/IEC 9899:2024 (-std=c23)
- Reduce gripes from cppcheck and other static analyzers
- Function rig_get_conf() is deprecated and will be removed in 5.0.
  Use rig_get_conf2() instead.
- (TBD)

## Build/install Changes
- POSIX thread(PTHREADS) support required
- C compiler - Supported compilers unchanged; c11 or c17 recommended,
  c23 optional. 5.0 will require at least c11.

## Changes for 5.0

### Storage restructuring
HAMLIB 5.0 will move many data items/structures out of rig_struct into
separate, calloc'ed buffers. This changes many of the methods for accessing
HAMLIB internal data.

### Include files

