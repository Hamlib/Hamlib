#!/bin/sh

# Builds Hamlib 4.x W32 binary distribution.

# A script to build a set of W32 binary DLLs and executables from a Hamlib
# source tarball.  This script assumes that the Hamlib source tarball has been
# extracted to the directory specified in $BUILD_DIR and that libusb-1.x.y has
# also been extracted to $BUILD_DIR.

# See README.build-Windows for complete details.

# running 'file' against the resulting .DLL should return the following or similar:
# file libhamlib-4.dll
# libhamlib-4.dll: PE32 executable for MS Windows 4.00 (DLL), Intel i386 (stripped to external PDB), 10 sections


# Set this to a desired directory
BUILD_DIR=~/builds

# Set this to LibUSB archive extracted in $BUILD_DIR
LIBUSB_VER=libusb-1.0.24

# Set to the correct HOST_ARCH= line for your MinGW installation
HOST_ARCH=i686-w64-mingw32

# Set to the strip name for your version of MinGW
HOST_ARCH_STRIP=i686-w64-mingw32-strip

# Set to the dlltool name for your version of MinGW
HOST_ARCH_DLLTOOL=i686-w64-mingw32-dlltool

# Error return codes.  See /usr/include/sysexits.h
EX_USAGE=64
EX_NOINPUT=66


# Pass name of Hamlib archive extracted in $BUILD_DIR
if test $# -ne 1
then
    echo
    echo "Usage: $(basename $0) hamlib-version"
    echo "See README.build-Windows for more information."
    echo

    exit ${EX_USAGE}
fi

# Make sure the Hamlib archive is where we expect
if test -d ${BUILD_DIR}/$1
then
    echo
    echo "Building W32 binaries in ${BUILD_DIR}/$1"
    echo

    cd ${BUILD_DIR}/$1
else
    echo
    echo "Build directory, ${BUILD_DIR}/$1 not found!"
    echo "Check path for $1 or correct the version number."
    echo

    exit ${EX_NOINPUT}
fi

RELEASE=$(/usr/bin/awk 'BEGIN{FS="["; RS="]"} /\[4\./ {print $2;exit}' ./configure.ac)
HL_FILENAME=hamlib-w32-${RELEASE}
INST_DIR=$(pwd)/mingw32-inst
ZIP_DIR=$(pwd)/${HL_FILENAME}
LIBUSB_1_0_BIN_PATH=${BUILD_DIR}/${LIBUSB_VER}


# Create W32 specific README.w32-bin file
cat > README.w32-bin <<END_OF_README
What is it?
===========

This ZIP archive or Windows installer contains a build of Hamlib-${RELEASE}
cross-compiled for MS Windows 32 bit using MinGW under Debian GNU/Linux 13
(nice, heh!).

This software is copyrighted. The library license is LGPL, and the *.EXE files
licenses are GPL.  Hamlib comes WITHOUT ANY WARRANTY. See the LICENSE.txt,
COPYING.txt, and COPYING.LIB.txt files.

Supporting documentation in the form of Unix manual pages have also been
included after being converted to HTML.


Installation and Configuration
==============================

Extract the ZIP archive into a convenient location, C:\Program Files is a
reasonable choice.

The archive directory structure is:

hamlib-w32-4.7~git
├── bin
├── doc
├── include
│   └── hamlib
└── lib
    ├── gcc
    └── msvc

The 'bin' and 'doc' directories will be of interest to users while developers
will be interested in the 'include' and 'lib' directories as well.

Make sure *all* the .DLL files are in your PATH (leave them in the 'bin'
directory and set the PATH).  To set the PATH environment variable in Windows
2000, Windows XP, and Windows 7 (need info on Vista and Windows 8/10) do the
following:

 * W2k/XP: Right-click on "My Computer"
   Win7: Right-click on "Computer"

 * W2k/XP: Click the "Advanced" tab of the "System Properties" dialog
   Win7: Click the "Advanced system settings" link in the System dialog

 * Click the "Environment Variables" button of the pop-up dialog

 * Select "Path" in the "System variables" box of the "Environment Variables"
   dialog

   NB: If you are not the administrator, system policy may not allow editing
   the path variable.  The complete path to an executable file will need to be
   given to run one of the Hamlib programs.

 * Click the Edit button

 * Now add the Hamlib path in the "Variable Value:" edit box.  Be sure to put
   a semi-colon ';' after the last path before adding the Hamlib path (NB. The
   entire path is highlighted and will be erased upon typing a character so
   click in the box to unselect the text first.  The PATH is important!!)
   Append the Hamlib path, e.g. C:\Program Files\hamlib-w32-${RELEASE}\bin

 * Click OK for all three dialog boxes to save your changes.


Testing with the Hamlib Utilities
=================================

To continue, be sure you have read the README.betatester file, especially the
"Testing Hamlib" section.  The primary means of testing is by way of the
rigctl utility for radios, the rotctl utility for rotators and the ampctl
utility for amplifiers.  Each is a command line program that is interactive
or can act on a single command and exit.

Documentation for each utility can be found as an HTML file in the doc
directory.

In short, the command syntax is of the form:

  rigctl -m 1020 -r COM1 -vvvvv

  -m -> Radio model 1020, or Yaesu FT-817 (use 'rigctl -l' for a list)
  -r -> Radio device, in this case COM1
  -v -> Verbosity level.  For testing four or five v characters are required.
        Five 'v's set a debug level of TRACE which generates a lot of screen
        output showing communication to the radio and values of important
        variables.  These traces are vital information for Hamlib rig backend
        development.

To run rigctl or rotctl open a cmd window (Start|Run|enter 'cmd' in the
dialog).  If text scrolls off the screen, you can scroll back with the mouse.
To copy output text into a mailer or editor (Notepad++, a free editor also
licensed under the GPL is recommended), highlight the text as a rectangle in
the cmd window, press <Enter> (or right-click the window icon in the upper left
corner and select Edit, then Copy), and paste it into your editor with Ctl-V
(or Edit|Paste from the typical GUI menu).

All feedback is welcome to the mail address below.


Uninstall
=========

To uninstall, simply delete the Hamlib directory.  You may wish to edit the
PATH as above to remove the Hamlib bin path, if desired.


Information for w32 Programmers
=================================

The DLL has a cdecl interface.

There is a libhamlib-4.def definition file for MS Visual C++/Visual Studio in
lib\msvc.  Refer to the recipe below to generate a local libhamlib-4.lib file
for use with the VC++/VS linker.

Simply '#include <hamlib/rig.h>' (or any other header) (add directory to
include path), include libhamlib-4.lib in your project and you are done.  Note:
VC++/VS cannot compile all the Hamlib code, but the API defined by rig.h has
been made MSVC friendly :-)

As the source code for the library DLLs is licensed under the LGPL, your
program is not considered a "derivative work" when using the published Hamlib
API and normal linking to the front-end library, and may be of a license of
your choosing.

As of 04 Aug 2025 a .lib file is generated using the MinGW dlltool utility.
This file is now installed to the lib\gcc directory.  As it is generated
by the MinGW dlltool utility, it is only suitable for use with MinGW/GCC.

For developers using Microsoft Visual Studio, the following recipe is
provided by Phil Rose, GM3ZZA:

My secret sauce is:

Open "Developer PowerShell for VS2022" in administrator mode. This adds the
correct directory to the path and allows update of "C:\Program Files" with the
.dll.

Then (in my case).

cd "C:\Program Files\hamlib-w32-${RELEASE}\lib\msvc"
lib /def:libhamlib-4.def /machine:x86

If you use any other terminal then the full path to lib.exe is needed
(today it is
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\lib.exe",
but is dependent on the version of MSVC which gets updated every few weeks).

NOTE: feedback is requested on Phil's example!  Please let know if this works
for you.

The published Hamlib API may be found at:

http://hamlib.sourceforge.net/manuals/4.1/index.html


Thank You!
==========

Patches, feedback, and contributions are welcome.

Please report problems or success to hamlib-developer@lists.sourceforge.net

Cheers,
Stephane Fillod - F8CFE
Mike Black - W9MDB (SK)
Nate Bargmann - N0NB
http://www.hamlib.org

END_OF_README


# Configure and build hamlib for i686-w64-mingw32, with libusb-1.0

./configure --host=${HOST_ARCH} \
 --prefix=${INST_DIR} \
 --without-cxx-binding \
 --disable-static \
 CPPFLAGS="-I${LIBUSB_1_0_BIN_PATH}/include" \
 LDFLAGS="-L${LIBUSB_1_0_BIN_PATH}/MinGW32/dll" \
 LIBUSB_CFLAGS="-I${LIBUSB_1_0_BIN_PATH}/include/libusb-1.0" \
 LIBUSB_LIBS="-lusb-1.0"



make -j 4 --no-print-directory install

mkdir -p ${ZIP_DIR}/bin ${ZIP_DIR}/lib/msvc ${ZIP_DIR}/lib/gcc ${ZIP_DIR}/include ${ZIP_DIR}/doc
cp -a src/libhamlib.def ${ZIP_DIR}/lib/msvc/libhamlib-4.def
todos ${ZIP_DIR}/lib/msvc/libhamlib-4.def
cp -a ${INST_DIR}/include/hamlib ${ZIP_DIR}/include/.
todos ${ZIP_DIR}/include/hamlib/*.h

# C++ binding is useless on w32 because of ABI
for f in *class.h
do
    rm ${ZIP_DIR}/include/hamlib/${f}
done

for f in AUTHORS ChangeLog COPYING COPYING.LIB LICENSE README.md README.betatester README.w32-bin THANKS
do
    cp -a ${f} ${ZIP_DIR}/${f}.txt
    todos ${ZIP_DIR}/${f}.txt
done

# Generate HTML documents from nroff formatted man files
for f in doc/man1/*.1 doc/man7/*.7
do
    /usr/bin/groff -mandoc -Thtml >${f}.html ${f}
    cp -a ${f}.html ${ZIP_DIR}/doc/.
done

cd ${BUILD_DIR}/$1

# Copy build files into specific locations for Zip file
for f in *.exe
do
    cp -a ${INST_DIR}/bin/${f} ${ZIP_DIR}/bin/.
done

cp -a ${INST_DIR}/bin/libhamlib-?.dll ${ZIP_DIR}/bin/.
cp -a ${INST_DIR}/lib/libhamlib.dll.a ${ZIP_DIR}/lib/gcc/.

# NB: Strip Hamlib DLLs and EXEs
${HOST_ARCH_STRIP} ${ZIP_DIR}/bin/*.exe ${ZIP_DIR}/bin/*hamlib-*.dll

# Copy needed third party DLLs
cp -a /usr/i686-w64-mingw32/lib/libwinpthread-1.dll ${ZIP_DIR}/bin/.
cp -a ${LIBUSB_1_0_BIN_PATH}/MinGW32/dll/libusb-1.0.dll ${ZIP_DIR}/bin/libusb-1.0.dll

# Required for MinGW with GCC 6.3 (Debian 9)
FILE="/usr/lib/gcc/i686-w64-mingw32/6.3-posix/libgcc_s_sjlj-1.dll"
if test -f "$FILE"
then
    cp -a ${FILE} ${ZIP_DIR}/bin/.
fi

# Required for MinGW with GCC 8.3 (Debian 10)
FILE="/usr/lib/gcc/i686-w64-mingw32/8.3-posix/libgcc_s_sjlj-1.dll"
if test -f "$FILE"
then
    cp -a ${FILE} ${ZIP_DIR}/bin/.
fi

# Required for MinGW with GCC 10 (Debian 11)
FILE="/usr/lib/gcc/i686-w64-mingw32/10-posix/libgcc_s_dw2-1.dll"
if test -f "$FILE"
then
    cp -a ${FILE} ${ZIP_DIR}/bin/.
fi

# Required for MinGW with GCC 12 (Debian 12)
FILE="/usr/lib/gcc/i686-w64-mingw32/12-posix/libgcc_s_dw2-1.dll"
if test -f "$FILE"
then
    cp -a ${FILE} ${ZIP_DIR}/bin/.
fi

# Required for MinGW with GCC 14 (Debian 13)
FILE="/usr/lib/gcc/i686-w64-mingw32/14-posix/libgcc_s_dw2-1.dll"
if test -f "$FILE"
then
    cp -a ${FILE} ${ZIP_DIR}/bin/.
fi

# Generate .lib file for GCC on MinGW per Jonathan Yong from mingw-w64
# https://sourceforge.net/p/mingw-w64/discussion/723798/thread/e23dceba20/?limit=25#51dd/3df2/3708/e62b
${HOST_ARCH_DLLTOOL} --input-def ${ZIP_DIR}/lib/msvc/libhamlib-4.def --output-lib ${ZIP_DIR}/lib/gcc/libhamlib-4.lib

/usr/bin/zip -r ${HL_FILENAME}.zip $(basename ${ZIP_DIR})
