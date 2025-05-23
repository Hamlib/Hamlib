Hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
         (C) Stephane Fillod 2000-2011
         (C) The Hamlib Group 2000-2022


The purpose of this project is to provide stable, flexible, shared libraries
that enable quicker development of Amateur Radio Equipment Control
Applications.
The master repository is https://github.com/Hamlib/Hamlib
Daily snapshots are available at https://n0nb.users.sourceforge.net/
Development happens on the github master (often by
merging feature branches) and each release has a release branch.

Many Amateur Radio Transceivers come with serial interfaces that allows
software to control the radio. This project will endeavour to provide shared
libraries that greatly simplify the application programmer's interaction
with radio equipment and other controllable devices such as rotators,
switches, etc.

Supported Radios
----------------

The Hamlib Wiki page, Supported Radios, contains a snapshot of the supported
radios at the time of the last Hamlib release.  Go to http://www.hamlib.org
to reach the Wiki.

Hamlib Design
-------------

The library provides functions for radio, rotator and amplifier control,
and data retrieval for supported devices.  A number of functions useful
for calculating distance and bearing and grid square conversion are included.

libhamlib.so -  library that provides generic API for all RIG types.
    This is what Application programmers will "see".  Will have different
    names on other platforms, e.g. libhamlib-2.dll on MS windows.  Also
    contains all radio, rotator and amplifier "backends" (formerly in their
    own dlopen'ed libraries) provided by Hamlib.

Backend Examples are:
---------------------

1. yaesu will provide connectivity to Yaesu FT 747GX Transceiver, FT 847
   "Earth Station", etc. via a standard API.

2. xxxx. will provide connectivity to the Wiz-bang moon-melter 101A (yikes..)

Hamlib will also enable developers to develop professional looking GUI's
towards a standard control library API, and they would not have to worry
about the underlying connection towards physical hardware.

Initially serial (RS232) connectivity will be handled, but we expect that IP
(and other) connectivity will follow afterwards.  Connection via a USB port
is accomplished via the Linux kernel support.  USB to serial converters are
well supported.  Other such devices may be supported as long as they present
a serial (RS-232) interface to Hamlib.

Availability
------------

Most distributions have the latest Hamlib release in their testing or alpha
versions of their distribution.  Check your package manager for the Hamlib
version included in your distribution.

Developing with Hamlib API
--------------------------

API documentation is at:

        https://github.com/Hamlib/Hamlib/wiki/Documentation

Take a look at tests/README for more info on simple programming examples and
test programs.

C++ programming is supported and language bindings are available for Perl,
Python, and TCL.  A network daemon utility is also available for any
programming language that supports network sockets (even netcat!).


Recompiling
-----------

Hamlib is entirely developed using GNU tools, under various Linux systems.
The library may be recompiled by the familiar "three step":

Note: if ./configure does not exist run ./bootstrap first

        ./configure
        make
        sudo make install

For debugging use this configure
        ./configure CFLAGS=-g -O0 -fPIC --no-create --no-recursio

See the INSTALL file for more information.

Contributing
------------

Consult the README.betatester and README.developer files in this directory
if you feel like testing or helping with Hamlib development.

Contributions of rig specifications and protocol documentation are highly
encouraged.  Do keep in mind that in some cases the manufacturer may not
provide complete control information or it is only available under a
Non-Disclosure Agreement (NDA).  Any documentation *must* be publicly
available so we can legally write and distribute Free Software supporting a
given device.

The Hamlib team is very interested to hear from you, how Hamlib builds and
works on your system, especially on non-Linux system or non-PC systems. We
try to make Hamlib as portable as possible.

Please report in case of problems at hamlib-developer@lists.sourceforge.net
Git email formatted patches or in unified diff format are welcome!

Also, take a look at http://sourceforge.net/projects/hamlib/ Here you will
find a mail list, link to the Wiki, and the latest releases.  Feedback,
questions, etc. about Hamlib are very welcome at the mail list:

        <hamlib-developer@lists.sourceforge.net>

Hamlib Version Numbers
----------------------

Like other software projects, Hamlib uses a version numbering scheme to help
program authors and users understand which releases are compatible and which
are not.  Hamlib releases now follow the format of:

Major.minor.incremental

Where

Major:  Currently at 4, but can be advanced when changes to the API require
client programs to be rewritten to take advantage of new features of
Hamlib.  This number has advanced a couple of times throughout the life of
Hamlib.  Advancement of the major number is only for frontend API changes
that require modification of client source.  ABI compatibility is presently
maintained to prior releases so that a program linked to an earlier
1.2.Y.[Z] release will work with a later 3.Y[.Z] release without
recompiling. It is our intention to maintain such ABI compatibility as long
as practical.

Minor:  This number advances when either new backend(s) or new rig
model(s) to existing backend(s) are added.  Advancing this number informs
client program authors (and users of those programs) that new model/backend
support has been added.  Will also include bug fixes since the last
Incremental release.

Incremental:    May be undefined (e.g. Hamlib 3.0) and would advance to 1
(e.g. Hamlib 3.0.1) for any bug fixes or feature additions to existing
model(s) or backend(s), then to 2, etc.  New rig models or backends are not
included in Incremental.  When Release is advanced, Incremental will reset
to undefined and will not be included as part of the version number.

Release schedule
----------------

Hamlib has in the past maintained a "ready when it's ready" philosophy.
However, given that much of the Linux user base is now influenced by the
Ubuntu distribution and its timed six month release schedule, Hamlib
releases will be scheduled in advance of Ubuntu releases.  Planned release
dates for Hamlib are now 1 February and 1 August of each calendar year.
Between those dates various Incremental releases will occur as development
warrants.


Have Fun / Frank S / Stephane F / The Hamlib Group

  73's de vk3fcs/km5ws / f8cfe

