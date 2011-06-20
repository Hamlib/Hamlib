#!/bin/bash

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
This ZIP archive contains a build of Hamlib-$RELEASE
cross-compiled for Win32 using MinGW under Debian GNU/Linux (nice, heh!).
The DLL has a cdecl interface for MS VC++.

This material is copyrighted. The library license is LGPL, and the *.EXE
files licenses are GPL.  Hamlib comes WITHOUT ANY WARRANTY. See LICENSE.txt
COPYING.txt, and COPYING.LIB.txt files.

Make sure *all* the .DLL are in your PATH, and you have read
the README.betatester file, especially the "testing Hamlib" section.

There's a .LIB import library for MS-VC++ in lib/msvc.  Simply #include
<hamlib/rig.h> (add directory to include path), include the .LIB in your
project and you're done. Note: MS-VC++ cannot compile all the Hamlib code,
but the API rig.h has been made MSVC friendly :-)

Patches, feedback, and contributions are welcome.

Please report problems, success to hamlib-developer@lists.sourceforge.net

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
 PKG_CONFIG_LIBDIR=${LIBUSB_WIN32_BIN_PATH}/lib/pkgconfig

make install

mkdir -p ${ZIP_DIR}/bin ${ZIP_DIR}/lib/msvc ${ZIP_DIR}/lib/gcc ${ZIP_DIR}/include
cp -a src/libhamlib.def ${ZIP_DIR}/lib/msvc/libhamlib-2.def; todos ${ZIP_DIR}/lib/msvc/libhamlib-2.def
cp -a ${INST_DIR}/include/hamlib ${ZIP_DIR}/include/.; todos ${ZIP_DIR}/include/hamlib/*.h

# C++ binding is useless on win32 because of ABI
rm ${ZIP_DIR}/include/hamlib/{rig,rot}class.h

for f in AUTHORS ChangeLog README README.betatester LICENSE COPYING COPYING.LIB README.win32-bin THANKS ; do \
	cp -a ${f} ${ZIP_DIR}/${f}.txt ; todos ${ZIP_DIR}/${f}.txt ; done

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
zip -r hamlib-win32-${RELEASE}-`date +%Y%m%d`.zip `basename ${ZIP_DIR}`


