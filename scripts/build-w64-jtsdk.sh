#!/bin/sh

# Builds Hamlib 4.x W64 dl binary distribution under Windows Qt MinGW/MSYS2 
# Customised for >= JTSDK 3.2.0 B3 stream(s)

# A script to compile a set of W64 binary DLLs, executables and compiler 
# packages from a Hamlib source tarball.  This script assumes uses the 
# JTSDK's previously deployed LibUSB (pointed to by JTSDK environment 
# variable $libusb_dir_f)

# See future README.build-JTSDK for complete details.

#Ensure that the Qt-supplied GCC compilers and tools can be found
export PATH=$PATH:$GCCD_F:.

# Set this to a desired directory
if [[ -z "${BUILD_BASE_DIR}" ]]; then
  BUILD_DIR=~/builds
else
  BUILD_DIR=~/${BUILD_BASE_DIR}
fi

# Set this to LibUSB archive extracted in $BUILD_DIR
LIBUSB_VER=libusb-1.0.24

# Set to the correct HOST_ARCH= line for your minGW installation
HOST_ARCH=x86_64-w64-mingw32

# Set to the strip name for your version of minGW
HOST_ARCH_STRIP=strip.exe

# Set to the name of the utility to provide Unix to DOS translation
UNIX_TO_DOS_TOOL=unix2dos.exe

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
    echo "Building W64 binaries in ${BUILD_DIR}/$1"
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
HL_FILENAME=hamlib-w64-${RELEASE}
INST_DIR=$(pwd)/mingw64-inst
ZIP_DIR=$(pwd)/${HL_FILENAME}
# LIBUSB_1_0_BIN_PATH=${BUILD_DIR}/${LIBUSB_VER} # REDUNDANT


# Create W64 specific README.w64-bin file
cat > README.w64-bin <<END_OF_README
What is it?
===========

This ZIP archive or Windows installer contains a build of Hamlib-$RELEASE
native compiled for MS Windows 64 bit using MinGW under Windows JTSDK 3.2.0
(nice, heh!).

This software is copyrighted. The library license is LGPL, and the *.EXE files
licenses are GPL.  Hamlib comes WITHOUT ANY WARRANTY. See the LICENSE.txt,
COPYING.txt, and COPYING.LIB.txt files.

Supporting documentation in the form of Unix manual pages have also been
included after being converted to HTML.


Installation and Configuration
==============================

Extract the ZIP archive into a convenient location, C:\Program Files (being x64)
is a reasonable choice.

Make sure *all* the .DLL files are in your PATH (leave them in the bin
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
   Append the Hamlib path, e.g. C:\Program Files\hamlib-w64-4.0~git\bin

 * Click OK for all three dialog boxes to save your changes.


Testing with the Hamlib Utilities
=================================

To continue, be sure you have read the README.betatester file, especially the
"Testing Hamlib" section.  The primary means of testing is by way of the
rigctl utility for radios and rotctl utility for rotators.  Each is a command
line program that is interactive or can act on a single command and exit.

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
To copy output text into a mailer or editor (I recommend Notepad++, a free
editor also licensed under the GPL), highlight the text as a rectangle in the
cmd window, press <Enter> (or right-click the window icon in the upper left
corner and select Edit, then Copy), and paste it into your editor with Ctl-V
(or Edit|Paste from the typical GUI menu).

All feedback is welcome to the mail address below.


Uninstall
=========

To uninstall, simply delete the Hamlib directory.  You may wish to edit the
PATH as above to remove the Hamlib bin path, if desired.


Information for w64 Programmers
=================================

The DLL has a cdecl interface.

There is a libhamlib-4.def definition file for MS Visual C++/Visual Studio in
lib/msvc.  Refer to the sample commands below to generate a local
libhamlib-4.lib file for use with the VC++/VS linker.

Simply #include <hamlib/rig.h> (add directory to include path), include
libhamlib-4.lib in your project and you are done.  Note: VC++/VS cannot
compile all the Hamlib code, but the API defined by rig.h has been made MSVC
friendly :-)

As the source code for the library DLLs is licensed under the LGPL, your
program is not considered a "derivative work" when using the published Hamlib
API and normal linking to the front-end library, and may be of a license of
your choosing.

For linking the library with MS Visual C++ 2003, from the directory you
installed Hamlib run the following commands to generate the libhamlib-4.lib
file needed for linking with your MSVC project:

cd lib\msvc
c:\Program Files\Microsoft Visual C++ Toolkit 2003\bin\link.exe /lib /machine:amd64 /def:libhamlib-4.def

To do the same for Visual Studio 2017:

cd lib\msvc
c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x86\bin\link.exe /lib /machine:i386 /def:libhamlib-4.def

and for VS 2019:

cd lib\msvc
c:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.25.28610\bin\Hostx64\x86\bin\link.exe /lib /machine:i386 /def:libhamlib-4.def

NOTE: feedback is requested on the previous two command examples as these do
not appear to be correct to generate a 64 bit libhamlib-4.lib file!

The published Hamlib API may be found at:

http://hamlib.sourceforge.net/manuals/4.1/index.html


Thank You!
==========

Patches, feedback, and contributions are welcome.

Please report problems or success to hamlib-developer@lists.sourceforge.net

Cheers,
Stephane Fillod - F8CFE
Nate Bargmann - N0NB
JTSDK Maintenance Team - JTSDK@GROUPS.IO
http://www.hamlib.org

END_OF_README


# Configure and build hamlib for x86_64-w64-mingw32, with libusb-1.0

./configure --host=${HOST_ARCH} \
 --prefix=${INST_DIR} \
 --without-cxx-binding \
 --disable-static \
 CPPFLAGS="-I${libusb_dir_f}/include" \
 LDFLAGS="-L${libusb_dir_f}/MinGW64/dll"


make -j ${CPUS} install

mkdir -p ${ZIP_DIR}/bin ${ZIP_DIR}/lib/msvc ${ZIP_DIR}/lib/gcc ${ZIP_DIR}/include ${ZIP_DIR}/doc
cp -a src/libhamlib.def ${ZIP_DIR}/lib/msvc/libhamlib-4.def
${UNIX_TO_DOS_TOOL} ${ZIP_DIR}/lib/msvc/libhamlib-4.def
cp -a ${INST_DIR}/include/hamlib ${ZIP_DIR}/include/.
${UNIX_TO_DOS_TOOL} ${ZIP_DIR}/include/hamlib/*.h

# C++ binding is useless on w64 because of ABI
for f in *class.h
do
    rm ${ZIP_DIR}/include/hamlib/${f}
done

for f in AUTHORS ChangeLog COPYING COPYING.LIB LICENSE README.md README.betatester README.w64-bin THANKS
do
    cp -a ${f} ${ZIP_DIR}/${f}.txt
    ${UNIX_TO_DOS_TOOL} ${ZIP_DIR}/${f}.txt
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
cp -a ${QTD_F}/libwinpthread-1.dll ${ZIP_DIR}/bin/.
cp -a ${libusb_dir_f}/MinGW64/dll/libusb-1.0.dll ${ZIP_DIR}/bin/libusb-1.0.dll

# Set for MinGW build - To Be safe !
FILE="${QTD_F}/libgcc_s_seh-1.dll"
if test -f "$FILE"
then
    cp -a ${FILE} ${ZIP_DIR}/bin/.
fi

# Copy over the main MSYS2 Runtime DLL (v2.0 at time of development)
# This is dirty
FILE="/usr/bin/msys-2.0.dll"
if test -f "$FILE"
then
    cp -a ${FILE} ${ZIP_DIR}/bin/.
fi

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

/usr/bin/zip -r ${HL_FILENAME}.zip $(basename ${ZIP_DIR})
