#!/bin/sh

# A script to build a set of VB.NET 2002 Framework 1.1 binary DLLs from a
# Hamlib tarball. This script assumes that the Hamlib tarball has been
# extracted to the directory specified in $build_dir and that
# libusb-win32-bin-1.x.y.z has also been extracted to $build_dir and a
# libusb.pc file has been created.  The MS VC++ Toolkit must also be
# installed.
#
# See README.build-VB.NET for complete details.


# Set this to a desired directory
BUILD_DIR=~/builds

# Set this to LibUSB archive extracted in $BUILD_DIR
LIBUSB_VER=libusb-win32-bin-1.2.4.0


# Error return codes.  See /usr/include/sysexits.h
EX_USAGE=64
EX_NOINPUT=66


# Pass name of Hamlib archive extracted in $BUILD_DIR
if [ $# -ne 1 ]; then
    echo
    echo "Usage: $(basename $0) hamlib-version"
    echo "See README.build-VB.NET for more information."

    exit $EX_USAGE
fi

# Make sure the Hamlib archive is where we expect
if [ -d ${BUILD_DIR}/$1 ]; then
    echo
    echo "Building VB.NET binaries in ${BUILD_DIR}/$1"
    echo

    cd ${BUILD_DIR}/$1
else
    echo "Build directory, ${BUILD_DIR}/$1 not found!"
    echo "Check path for $1 or correct the version number."

    exit $EX_NOINPUT
fi

# FIXME: Determine RELEASE only from AC_INIT line to avoid any other similar
# values and avoid hard coded version number.
RELEASE=$(/usr/bin/awk 'BEGIN{FS="["; RS="]"} /\[3\./ {print $2}' ./configure.ac)
INST_DIR=$(pwd)/mingw-inst
ZIP_DIR=$(pwd)/hamlib-VB.NET-${RELEASE}
LIBUSB_WIN32_BIN_PATH=${BUILD_DIR}/${LIBUSB_VER}


# Create VB.NET specific README.VB.NET-bin file
cat > README.VB.NET-bin <<END_OF_README
What is it?
===========

This ZIP archive contains a build of Hamlib-$RELEASE
cross-compiled for VB.NET 2002 Framework 1.1 using MinGW under
Debian GNU/Linux (nice, heh!).

The DLL has a stdcall interface for MS VB.NET 2002 Framework 1.1.

This material is copyrighted. The library license is LGPL, and the *.EXE
files licenses are GPL.  Hamlib comes WITHOUT ANY WARRANTY. See LICENSE.txt
COPYING.txt, and COPYING.LIB.txt files.


Usage
=====

The following originates from a mail by Michael Benz who did the work. It
explains how to proceed to have the VB Wrapper used with VB.NET 2002 with
Framework 1.1 (others not tested)

------------------------------------------------------------------------------

In the newer VB Modules (.BAS) don't exist anymore. So they were replaced by
the Classes (.VB) The Wrapper is not in final Condition, many DLL Function
are still not covered now, but it will be a good start.

To Import the "Wrapper" use the Folder "Project" and "Import existing
Element" Now import the Class "Hamlib.VB". It alsough seems to import into
Sharpdevelop, so anybody can try it out for free!

http://www.icsharpcode.net/OpenSource/SD/Default.aspx

This Class contains Your Enumeration as well as the DLLImport to get Access
to the Hamlib DLL.

Covered are:
	- Init Rig with Comport and Speed
	- set/ get Frequency
	- set/get Mode
	- set/get VFO
	- get Riginfo

	-rig_debug_level_e not verified  (Function is Void, VB Dokumentation
says this is not possible to be marshaled, but Compiler is still accapting this)


To get access to the Class you have to add something like this in your Main Class
    Dim RigLib As RigControll = New RigControll   'get Access to RigLib Klass



here is an Example how to use the Class:

Private Sub Button2_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button2.Click
        'Dim tokenlookup As String = "rig_pathname"
        'Dim tokenlookup As Object = "serial_speed"
        'Dim Info As String
        Dim Frequenz As Double
        Dim ZeichenOut As String
        Dim TokenPointer As System.Int32
        Dim VFO As Integer
        Dim Mode As RigControll.RMode_t
        Dim Bandbreite As Long
        ' Dim RigLib As RigControll = New RigControll

        Button3.Enabled() = True
        Button2.Enabled() = False


RigLib.rig_set_debug(RigControll.rig_debug_level_e.RIG_DEBUG_TRACE)
        myrig = RigLib.rig_init(RigNumber.Text)

        TBmyrig.Text = myrig.ToString   'convert myrig to String

        TokenPointer = RigLib.rig_token_lookup(myrig, "rig_pathname")
        TBZeichen.Text = RigLib.rig_set_conf(myrig, TokenPointer, ComboBox2.Text)
        Token.Text = TokenPointer

        TokenPointer = RigLib.rig_token_lookup(myrig, "serial_speed")
        TBZeichen.Text = RigLib.rig_set_conf(myrig, TokenPointer, ComboBox3.Text)
        Token.Text = TokenPointer

        RigLib.rig_open(myrig)
        TB_Riginfo.Text = RigLib.rig_get_info(myrig)

        RigLib.rig_get_vfo(myrig, VFO)
        TextBox8.Text = VFO.ToString

        RigLib.rig_get_freq(myrig, VFO, Frequenz)
        TextBox2.Text() = Frequenz

        RigLib.rig_get_mode(myrig, VFO, Mode, Bandbreite)
        TextBox3.Text = [Enum].GetName(GetType(RigControll.RMode_t), Mode)
        TextBox4.Text = Bandbreite

 End Sub

+++++++++++++++++++++++++++++++++

kind Regard

Michael

------------------------------------------------------------------------------


Thank You!
==========

Patches, feedback, and contributions are welcome.

Please report problems, success to hamlib-developer@lists.sourceforge.net

Cheers,
Stephane Fillod - F8CFE
Nate Bargmann - N0NB
http://www.hamlib.org

END_OF_README


# Edit include/hamlib/rig_dll.h for __stdcall
mv include/hamlib/rig_dll.h include/hamlib/rig_dll.h.orig
sed -e 's/__cdecl/__stdcall/' <include/hamlib/rig_dll.h.orig >include/hamlib/rig_dll.h
rm include/hamlib/rig_dll.h.orig

# Configure and build hamlib for mingw32, with libusb-win32

./configure --host=i586-mingw32msvc \
 --prefix=$(pwd)/mingw-inst \
 --without-cxx-binding \
 PKG_CONFIG_LIBDIR=${LIBUSB_WIN32_BIN_PATH}/lib/pkgconfig

make install

mkdir -p ${ZIP_DIR}/bin ${ZIP_DIR}/lib/msvc ${ZIP_DIR}/lib/gcc ${ZIP_DIR}/include
cp -a src/libhamlib.def ${ZIP_DIR}/lib/msvc/libhamlib-2.def; todos ${ZIP_DIR}/lib/msvc/libhamlib-2.def
cp -a ${INST_DIR}/include/hamlib ${ZIP_DIR}/include/.; todos ${ZIP_DIR}/include/hamlib/*.h

# C++ binding is useless on win32 because of ABI
for f in *class.h ; do \
    rm ${ZIP_DIR}/include/hamlib/${f}
done

for f in AUTHORS ChangeLog COPYING COPYING.LIB LICENSE README.md README.betatester README.VB.NET-bin THANKS ; do \
    cp -a ${f} ${ZIP_DIR}/${f}.txt ; todos ${ZIP_DIR}/${f}.txt ; done

# Copy build files into specific locations for Zip file
cp -a ${INST_DIR}/lib/hamlib/hamlib-*.dll ${ZIP_DIR}/bin/.
cp -a ${INST_DIR}/bin/libhamlib-?.dll ${ZIP_DIR}/bin/.
cp -a ${INST_DIR}/lib/libhamlib.dll.a ${ZIP_DIR}/lib/gcc/.

# NB: Do not strip libusb0.dll
i586-mingw32msvc-strip ${ZIP_DIR}/bin/*.exe ${ZIP_DIR}/bin/*hamlib-*.dll
cp -a ${LIBUSB_WIN32_BIN_PATH}/bin/x86/libusb0_x86.dll ${ZIP_DIR}/bin/libusb0.dll

# Need VC++ free toolkit installed (default Wine directory installation shown)
( cd ${ZIP_DIR}/lib/msvc/ && wine ~/.wine/drive_c/Program\ Files/Microsoft\ Visual\ C++\ Toolkit\ 2003/bin/link.exe /lib /machine:i386 /def:libhamlib-2.def )
zip -r hamlib-VB.NET-${RELEASE}.zip $(basename ${ZIP_DIR})
