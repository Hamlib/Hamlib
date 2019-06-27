#!/bin/bash

# Builds Hamlib 3.x W64 binary distribution.

# A script to build a set of W64 binary DLLs from a Hamlib tarball.
# This script assumes that the Hamlib tarball has been extracted to the
# directory specified in $BUILD_DIR and that libusb-win32-bin-1.x.y.z has also
# been extracted to $BUILD_DIR.  The MS VC++Toolkit must also be installed
# and working with Wine.
#
# Requires libusb-1.0 to be accessible for USB backends to be built.

# See README.build-w64 for complete details.


# Set this to a desired directory
BUILD_DIR=~/builds

# Set this to LibUSB archive extracted in $BUILD_DIR
LIBUSB_VER=libusb-1.0.20

# uncomment the correct HOST_ARCH= line for your minGW installation
HOST_ARCH=x86_64-w64-mingw32

# Set to the strip name for your version of minGW
HOST_ARCH_STRIP=x86_64-w64-mingw32-strip

# Error return codes.  See /usr/include/sysexits.h
EX_USAGE=64
EX_NOINPUT=66


# Pass name of Hamlib archive extracted in $BUILD_DIR
if [ $# -ne 1 ]; then
	echo -e "\nUsage: `basename $0` hamlib-version\n"
	echo -e "See README.build-w64 for more information.\n"
	exit ${EX_USAGE}
fi

# Make sure the Hamlib archive is where we expect
if [ -d ${BUILD_DIR}/$1 ]; then
	echo -e "\nBuilding w64 binaries in ${BUILD_DIR}/$1\n\n"
	cd ${BUILD_DIR}/$1
else
	echo -e "\nBuild directory, ${BUILD_DIR}/$1 not found!\nCheck path for $1 or correct the version number.\n"
	exit $EX_NOINPUT
fi

RELEASE=`/usr/bin/awk 'BEGIN{FS="["; RS="]"} /\[4\./ {print $2;exit}' ./configure.ac`
HL_FILENAME=hamlib-w64-${RELEASE}
INST_DIR=`pwd`/mingw64-inst
ZIP_DIR=`pwd`/${HL_FILENAME}
LIBUSB_1_0_BIN_PATH=${BUILD_DIR}/${LIBUSB_VER}


# Create W64 specific README.w64-bin file
cat > README.w64-bin <<END_OF_README
What is it?
===========

This ZIP archive or Windows installer contains a build of Hamlib-$RELEASE
cross-compiled for MS Windows 64 bit using MinGW under Debian GNU/Linux 8
(nice, heh!).

NB:  This Windows 64 bit release is EXPERIMENTAL!  Some features such as USB
backends have been disabled at this time.  Please report bugs, failures, and
success to the Hamlib mailing list below.

This software is copyrighted. The library license is LGPL, and the *.EXE
files licenses are GPL.  Hamlib comes WITHOUT ANY WARRANTY. See the
LICENSE.txt, COPYING.txt, and COPYING.LIB.txt files.

A draft user manual in HTML format is included in the doc directory.  Supporting
documentation in the form of Unix manual pages have also been included after
being converted to HTML.


Installation and Configuration
==============================

Extract the ZIP archive into a convenient location, C:\Program Files is a
reasonable choice.

Make sure *all* the .DLL files are in your PATH (leave them in the bin
directory and set the PATH).  To set the PATH environment variable in
Windows 2000, Windows XP, and Windows 7 (need info on Vista and Windows 8/10)
do the following:

 * W2k/XP: Right-click on "My Computer"
   Win7: Right-click on "Computer"

 * W2k/XP: Click the "Advanced" tab of the "System Properties" dialog
   Win7: Click the "Advanced system settings" link in the System dialog

 * Click the "Environment Variables" button of the pop-up dialog

 * Select "Path" in the "System variables" box of the "Environment Variables"
   dialog

   NB: If you are not the administrator, system policy may not allow editing
   the path variable.  The complete path to an executable file will need to
   be given to run one of the Hamlib programs.

 * Click the Edit button

 * Now add the Hamlib path in the "Variable Value:" edit box.  Be sure to put
   a semi-colon ';' after the last path before adding the Hamlib path (NB. The
   entire path is highlighted and will be erased upon typing a character so
   click in the box to unselect the text first.  The PATH is important!!)
   Append the Hamlib path, e.g. C:\Program Files\hamlib-w64-3.0~git\bin

 * Click OK for all three dialog boxes to save your changes.


Testing with the Hamlib Utilities
=================================

To continue, be sure you have read the README.betatester file, especially
the "Testing Hamlib" section.  The primary means of testing is by way of the
rigctl utility for radios and rotctl utility for rotators.  Each is a command
line program that is interactive or can act on a single command and exit.

Documentation for each utility can be found as a PDF in the pdf/ directory.

In short, the command syntax is of the form:

  rigctl -m 120 -r COM1 -vvvvv

  -m -> Radio model 120, or Yaesu FT-817 (use 'rigctl -l' for a list)
  -r -> Radio device, in this case COM1
  -v -> Verbosity level.  For testing four or five v characters are required.
        Five 'v's set a debug level of TRACE which generates a lot of screen
        output showing communication to the radio and values of important
        variables.  These traces are vital information for Hamlib rig backend
        development.

To run rigctl or rotctl open a cmd window (Start|Run|enter 'cmd' in the dialog).
If text scrolls off the screen, you can scroll back with the mouse.  To copy
output text into a mailer or editor (I recommend Notepad++, a free editor also
licensed under the GPL), highlight the text as a rectangle in the cmd window,
press <Enter> (or right-click the window icon in the upper left corner and
select Edit, then Copy), and paste it into your editor with Ctl-V (or
Edit|Paste from the typical GUI menu).

All feedback is welcome to the mail address below.


Uninstall
=========

To uninstall, simply delete the Hamlib directory.  You may wish to edit the
PATH as above to remove the Hamlib bin path, if desired.


Information for w64 Programmers
=================================

As the source code for the library DLLs is licensed under the LGPL, your
program is not considered a "derivative work" when using the published
Hamlib API and normal linking to the front-end library, and may be of a
license of your choosing.  The published Hamlib API may be found at:

http://hamlib.sourceforge.net/manuals/1.2.15/index.html

(The 3.0 API is unchanged although new documentation will be forthcoming.)


Thank You!
==========

Patches, feedback, and contributions are welcome.

Please report problems or success to hamlib-developer@lists.sourceforge.net

Cheers,
Stephane Fillod - F8CFE
Nate Bargmann - N0NB
http://www.hamlib.org

END_OF_README


# Configure and build hamlib for x86_64-w64-mingw32, with libusb-1.0

./configure --host=${HOST_ARCH} \
 --prefix=${INST_DIR} \
 --without-cxx-binding \
 --disable-static \
 CPPFLAGS="-I${LIBUSB_1_0_BIN_PATH}/include" \
 LDFLAGS="-L${LIBUSB_1_0_BIN_PATH}/MinGW64/dll"


make install

mkdir -p ${ZIP_DIR}/bin  ${ZIP_DIR}/lib/gcc ${ZIP_DIR}/include ${ZIP_DIR}/doc ${ZIP_DIR}/lib/msvc # ${ZIP_DIR}/pdf
cp -a src/libhamlib.def ${ZIP_DIR}/lib/msvc/libhamlib-2.def; todos ${ZIP_DIR}/lib/msvc/libhamlib-2.def
cp -a ${INST_DIR}/include/hamlib ${ZIP_DIR}/include/.; todos ${ZIP_DIR}/include/hamlib/*.h
cp -a doc/Hamlib_design.png ${ZIP_DIR}/doc
cp -a doc/hamlib.html ${ZIP_DIR}/doc

# C++ binding is useless on w64 because of ABI
rm ${ZIP_DIR}/include/hamlib/{rig,rot}class.h

for f in AUTHORS ChangeLog COPYING COPYING.LIB LICENSE README README.betatester README.w64-bin THANKS ; do \
    cp -a ${f} ${ZIP_DIR}/${f}.txt ; todos ${ZIP_DIR}/${f}.txt ; done

# Generate HTML documents from nroff formatted man files
for f in doc/man1/*.1 doc/man7/*.7; do \
    /usr/bin/groff -mandoc -Thtml >${f}.html ${f}
    cp -a ${f}.html ${ZIP_DIR}/doc/. ; done

cd ${BUILD_DIR}/$1

# Copy build files into specific locations for Zip file
cp -a ${INST_DIR}/bin/{rigctld.exe,rigctl.exe,rigmem.exe,rigsmtr.exe,rigswr.exe,rotctld.exe,rotctl.exe,rigctlcom.exe,ampctl.exe,ampctld.exe} ${ZIP_DIR}/bin/.
cp -a ${INST_DIR}/bin/libhamlib-?.dll ${ZIP_DIR}/bin/.
cp -a ${INST_DIR}/lib/libhamlib.dll.a ${ZIP_DIR}/lib/gcc/.

# NB: Strip Hamlib DLLs and EXEs
${HOST_ARCH_STRIP} ${ZIP_DIR}/bin/*.exe ${ZIP_DIR}/bin/*hamlib-*.dll

# Copy needed third party DLLs
cp -a /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll ${ZIP_DIR}/bin/.
cp -a ${LIBUSB_1_0_BIN_PATH}/MinGW64/dll/libusb-1.0.dll ${ZIP_DIR}/bin/libusb-1.0.dll

# Required for MinGW with GCC 6.3
cp -a /usr/lib/gcc/x86_64-w64-mingw32/6.3-posix/libgcc_s_seh-1.dll ${ZIP_DIR}/bin/libgcc_s_seh-1.dll

## Need VC++ free toolkit installed (default Wine directory installation shown)
( cd ${ZIP_DIR}/lib/msvc/ && wine ~/.wine/drive_c/Program\ Files/Microsoft\ Visual\ C++\ Toolkit\ 2003/bin/link.exe /lib /machine:amd64 /def:libhamlib-2.def )

zip -r ${HL_FILENAME}.zip `basename ${ZIP_DIR}`
