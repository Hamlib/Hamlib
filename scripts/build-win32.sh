#!/bin/bash

# Builds Hamlib 1.2.x Win32 binary distribution.

# A script to build a set of Win32 binary DLLs from a Hamlib tarball.
# This script assumes that the Hamlib tarball has been extracted to the
# directory specified in $build_dir and that libusb-win32-bin-1.x.y.z has also
# been extracted to $build_dir and a libusb.pc file has been created.  The
# MS VC++Toolkit must also be installed.
#
# See README.build-win32 for complete details.


# Set this to a desired directory
BUILD_DIR=~/builds

# Set this to LibUSB archive extracted in $BUILD_DIR
LIBUSB_VER=libusb-win32-bin-1.2.4.0


# Error return codes.  See /usr/include/sysexits.h
EX_USAGE=64
EX_NOINPUT=66


# Pass name of Hamlib archive extracted in $BUILD_DIR
if [ $# -ne 1 ]; then
	echo -e "\nUsage: `basename $0` hamlib-version\n"
	echo -e "See README.build-win32 for more information.\n"
	exit $EX_USAGE
fi

# Make sure the Hamlib archive is where we expect
if [ -d ${BUILD_DIR}/$1 ]; then
	echo -e "\nBuilding Win32 binaries in ${BUILD_DIR}/$1\n\n"
	cd ${BUILD_DIR}/$1
else
	echo -e "\nBuild directory, ${BUILD_DIR}/$1 not found!\nCheck path for $1 or correct the version number.\n"
	exit $EX_NOINPUT
fi

RELEASE=`/usr/bin/awk 'BEGIN{FS="["; RS="]"} /\[1\./ {print $2}' ./configure.ac`
INST_DIR=`pwd`/mingw-inst
ZIP_DIR=`pwd`/hamlib-win32-${RELEASE}
LIBUSB_WIN32_BIN_PATH=${BUILD_DIR}/${LIBUSB_VER}


# Create Win32 specific README.win32_bin file
cat > README.win32-bin <<END_OF_README
What is it?
===========

This ZIP archive or Windows installer contains a build of Hamlib-$RELEASE
cross-compiled for Win32 using MinGW under Xubuntu Linux 10.10 (nice, heh!).

The DLL has a cdecl interface for MS VC++.

This software is copyrighted. The library license is LGPL, and the *.EXE
files licenses are GPL.  Hamlib comes WITHOUT ANY WARRANTY. See the
LICENSE.txt, COPYING.txt, and COPYING.LIB.txt files.


Installation and Configuration
==============================

Extract the ZIP archive into a convenient location, C:\Program Files is a
reasonable choice.

Make sure *all* the .DLL files are in your PATH (leave them in the bin
directory and set the PATH).  To set the PATH environment variable in
Windows 2000, Windows XP, and Windows 7 (need info on Vista) do the
following:

 * W2k/XP: Right-click on "My Computer"
   Win7: Right-click on "Computer"

 * Select "Properties" from the pop-up menu

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
   Append the Hamlib path, e.g. C:\Program Files\hamlib-win32-1.2.14~git\bin

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

Information for Win32 Programmers
=================================

There is a .LIB import library for MS-VC++ in lib/msvc.  Simply #include
<hamlib/rig.h> (add directory to include path), include the .LIB in your
project and you are done. Note: MS-VC++ cannot compile all the Hamlib code,
but the API defined by rig.h has been made MSVC friendly :-)

As the source code for the library DLLs is licensed under the LGPL, your
program is not considered a "derivative work" when using the published
Hamlib API and normal linking to the front-end library, and may be of a
license of your choosing.  The published Hamlib API may be found at:

http://sourceforge.net/apps/mediawiki/hamlib/index.php?title=Documentation


Thank You!
==========

Patches, feedback, and contributions are welcome.

Please report problems or success to hamlib-developer@lists.sourceforge.net

Cheers,
Stephane Fillod - F8CFE
Nate Bargmann - N0NB
http://www.hamlib.org

END_OF_README


# Import internal ./libltdl and build it for mingw32
libtoolize --ltdl
cd libltdl; ./configure --host=i586-mingw32msvc && make; cd ..

# Configure and build hamlib for mingw32, with libusb-win32

./configure --disable-static \
 --host=i586-mingw32msvc \
 --prefix=`pwd`/mingw-inst \
 --without-rpc-backends \
 --without-cxx-binding \
 PKG_CONFIG_LIBDIR=${LIBUSB_WIN32_BIN_PATH}/lib/pkgconfig \
 LIBTOOL=./libltdl/libtool

make install

mkdir -p ${ZIP_DIR}/bin ${ZIP_DIR}/lib/msvc ${ZIP_DIR}/lib/gcc ${ZIP_DIR}/include ${ZIP_DIR}/pdf
cp -a src/libhamlib.def ${ZIP_DIR}/lib/msvc/libhamlib-2.def; todos ${ZIP_DIR}/lib/msvc/libhamlib-2.def
cp -a ${INST_DIR}/include/hamlib ${ZIP_DIR}/include/.; todos ${ZIP_DIR}/include/hamlib/*.h

# C++ binding is useless on win32 because of ABI
rm ${ZIP_DIR}/include/hamlib/{rig,rot}class.h

for f in AUTHORS ChangeLog COPYING COPYING.LIB LICENSE README README.betatester README.win32-bin THANKS ; do \
	cp -a ${f} ${ZIP_DIR}/${f}.txt ; todos ${ZIP_DIR}/${f}.txt ; done

# Generate PDF documents from nroff formatted man files
cd tests

for f in rigctl.1 rigctld.8 rigmem.1 rigsmtr.1 rigswr.1 rotctl.1 rotctld.8 ; do \
	groff -mandoc >${f}.ps ${f} ; ps2pdf ${f}.ps ; rm ${f}.ps ; \
    cp -a ${f}.pdf ${ZIP_DIR}/pdf/. ; done

cd ${BUILD_DIR}/$1

# Copy build files into specific locations for Zip file
cp -a ${INST_DIR}/bin/{rigctld.exe,rigctl.exe,rigmem.exe,rigsmtr.exe,rigswr.exe,rotctld.exe,rotctl.exe} ${ZIP_DIR}/bin/.
cp -a ${INST_DIR}/lib/hamlib/hamlib-*.dll ${ZIP_DIR}/bin/.
cp -a ${INST_DIR}/bin/libhamlib-?.dll ${ZIP_DIR}/bin/.
cp -a ${INST_DIR}/lib/libhamlib.dll.a ${ZIP_DIR}/lib/gcc/.

# NB: Do not strip libusb0.dll
i586-mingw32msvc-strip ${ZIP_DIR}/bin/*.exe ${ZIP_DIR}/bin/*hamlib-*.dll
cp -a ${LIBUSB_WIN32_BIN_PATH}/bin/x86/libusb0_x86.dll ${ZIP_DIR}/bin/libusb0.dll

# Need VC++ free toolkit installed (default Wine directory installation shown)
( cd ${ZIP_DIR}/lib/msvc/ && wine ~/.wine/drive_c/Program\ Files/Microsoft\ Visual\ C++\ Toolkit\ 2003/bin/link.exe /lib /machine:i386 /def:libhamlib-2.def )
zip -r hamlib-win32-${RELEASE}.zip `basename ${ZIP_DIR}`

