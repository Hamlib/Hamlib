==============================================================
JTSDK DLL Qt MinGW/MSYS2 Supplementary Notes
==============================================================

This assumes you have an environment set up that conforms with 
instructions in the WSJT-X Install notes that will build Hamlib 

Before you start.... RTFM at <hamlib-base>/scripts/README.build-Windows-JTSDK.txt


1. Update Environ
=================

pacman -Syuu


2. Deploy MinGW
===============

Ensure that the ZIP, DOS2UNIX and GROFF packages are deployed 
to and owrking in the MSYS2 Environment

pacman -S zip
pacman -S dos2unix
alias unix2dos='todos'
pacman -S groff

The next step is possibly redudnant as it has been incorporated
into build-w64-jtsdk.sh

export PATH=$PATH:$GCCD_F:.


3. Create a dir $HOME/Builds
============================

Open a MSYS2 Terminal fro a jtsdk64.ps1 environment 

In a JTSDK63 ENvironment type: 

msys2

In the msys2 enviro nment type:

cd ~		<== ensure you at home
mkdir builds
cd builds


4. Locate and unzip LibUSB matching version deployed above into builds
======================================================================

These steps in the original build-w64.sh script are redundant.

You may skip this step.

JTSDK 3.2.0 (and later) points the environment to the libusb 
deployment in X:\JTSDK64-Tools\tools\libusb through 
environment variable $libusb_dir_f .

If you need the source for LinUSB use steps similar to those
below to obtain source:  

wget https://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.24/libusb-1.0.24.tar.bz2
tar -xvf libusb-1.0.24.tar.bz2

[ This creates a subdirectory libusb-1.0.24 ]


5. Obtain latest Hamlib sourceforge
===================================

git clone git://git.code.sf.net/p/hamlib/code hamlib-4.2~git


6. Start the Process Rolling
============================

cd ./hamlib-4.2~git
./bootstrap			       <== Not included in Nate's notes !
./scripts/build-w64-jtsdk.sh hamlib-4.2~git


7. Tadaa - Drumroll !
=====================

==> Package in ~/build/hamlib-4.2~git/hamlib-w64-4.2~git as hamlib-w64-4.2~git.zip
==> Headers in ~/build/hamlib-4.2~git/hamlib-w64-4.2~git/include
==> Library in ~/build/hamlib-4.2~git/hamlib-w64-4.2~git/lib/gcc as libhamlib.dll.a (rename to libhamlib.dll for application)

