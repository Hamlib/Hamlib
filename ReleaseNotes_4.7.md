# HAMLIB 4.7 - Release Notes                        2026-02-15

This release brings new equipment support, bug fixes/code cleanups, and some
changes to ease transition to 5.0

As many changes are planned for Hamlib in the future leading to 5.0.0
(no planned release date at this time), the 4.7.x series is planned to
be a sort of long term support release and will receive updates in the
form of new models and backends as they are developed.  If all goes well
such updates will be in parallel with 5.0.0 development.

This will place a burden on contributors who are asked to submit such
code to both the master and Hamlib-4.7 branches.  Hopefully this period
will not last overly long and development can concentrate solely on the
Hamlib 5 series at some point in the future.

## New equipment support
- Icom IC-7300MK2
- Yaesu FTX-1 Optima/Field
- Drake R8/R8A/R8B - new, revised backend
- AF6SA WRC rotator
- GUOHETEC PMR-171 & Q900 moved to separate backend

## Bug fixes & code cleanups
- Codebase conforms to ISO/IEC 9899:2024 (-std=c23)
- Reduce gripes from cppcheck and other static analyzers
- Functions `rig_get_conf()`, `rot_get_conf()` and `amp_get_conf()` are deprecated and
  will be removed in 5.0. Use `..._get_conf2()` instead. See issue
  [#924](https://github.com/Hamlib/Hamlib/issues/924).
- Functions `rig_set_trn()` & `rig_get_trn()` deprecated; operation now handled internally.
- Documentation brought up to date.
- Many bug fixes - see NEWS for summary, `git log` or `gitg`/`gitk` for details.

## Build/install changes
- POSIX threads(PTHREADS) support required
- Many fixes for building the optional language bindings
- C compiler - Supported compilers unchanged; c11 or c17 recommended,
  c23 optional. 5.0 will require at least c11.

## Changes for 5.0
HAMLIB 5.0 will make some major changes to the Application Binary Interface(ABI) that will
require changes to some applications, and at least recompilation/linking for all
apps. These source changes can be made/tested/debugged incrementally with 4.7,
making the transition much easier. The Application Programming Interface(API)
does not change.

### Storage restructuring
HAMLIB 5.0 will move many data items/structures out of rig_struct into separate heap
buffers. See issues [#487](https://github.com/Hanlib/Hamlib/issues/487),
[#1445](https://github.com/Hamlib/Hamlib/issues/1445), and
[#1420](https://github.com/Hamlib/Hamlib/issues/1420). This changes many of
the methods for accessing HAMLIB internal data. HAMLIB 4.7 supports both methods,
supplying macros to use for forward compatibility.

### Include files
Along with the moves to separate storage, the definitions of these data structures
will be moved out of hamlib/rig.h to their own include files. These `.h` files also
define macros to get the address of said structures.

Preliminary versions of these files are also part of 4.7 for pre-emptive use.

### The good news
If your application only calls the Hamlib API routines, then nothing needs to change.
If your application only uses configuration items like file/port names, speeds, etc, it
may be easiest to change to using rig_set_conf(). and let Hamlib handle the internals.
