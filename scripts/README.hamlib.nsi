This script is the "Makefile" for the Nullsoft Scriptable Install System
(nsis), a system to create a user friendly GUI installer.

The script is written with the assumption that this script is placed in
a sibling directory of the Hamlib source which has the Win32 binaries
built by the build-win32.sh script:

$ tree -L 1
.
├── hamlib-1.2.15.3~rc1
├── libusb-win32-bin-1.2.4.0
└── nsis

3 directories, 0 files

From the nsis directory, invoke the nsis make utility as:

makensis -DRELEASE=1.2.15.3~rc1 hamlib.nsi

This will create the GUI installer from the Win32 binary files built by
the build-win32.sh script.

The installer features selectable Programs and Development options.  The
associated DLLs and text files are always installed (useful for
providing support files for progams linked to Hamlib).  The Programs
option installs the Hamlib executable programs and their PDF
documentation.  The Development option installs the GCC and MSVC++
linkable libraries and Hamlib header files.  In every case the installer
presents the LICENSE.txt file providing copyright, GPL/LGPL boilerplate,
and Hamlib resource links.

At this time the installer does not attempt to modify the PATH variable,
which is possible, due to a 1024 character limit in the installer.  It
is likely better to err on the side of safety than to trash a PATH
setting.  Perhaps testing the length of the existing path and assuring
the edited PATH does not exceed 1024 characters may be implemented in
the future.

At this time, an uninstaller is not implemented.  Merely delete the
directory Hamlib was installed and, edit the PATH as outlined in
README.win32-bin.txt if desired.

Nate Bargmann, N0NB
http://www.hamlib.org
