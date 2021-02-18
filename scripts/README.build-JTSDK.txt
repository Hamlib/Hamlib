==============================================================
JTSDK DLL Qt MinGW/MSYS2 Supplementary Notes
==============================================================

These instructions should work with JTSDk 3.2.0 Beta 3 and later.

The JTSDK 3.2.0 Stream (and later) is available at 
https://sourceforge.net/projects/hamlib-sdk/files/Windows/

Future updates to the JTSDK may implement these steps into the
MSYS2 "menu" command.


1. Update Environment
=====================

$ pacman -Syuu


2. Deploy MinGW
===============

Ensure that the 'zip' , 'dos2unix' and 'groff' packages are deployed 
to and working in the MSYS2 Environment

** Versions of JTSDK 3.2.0 Beta 4 and later will incorporate **
** deployment of these tools into the Setup scripts. **

$ pacman -S zip
$ pacman -S dos2unix
$ pacman -S groff

** The next step may be redundant as it has been incorporated **
** into build-w64-jtsdk.sh **

$ export PATH=$PATH:$GCCD_F:.


3. Create a dir $HOME/Builds
============================

Open a MSYS2 Terminal from a jtsdk64.ps1 environment 

In a JTSDK PowerShell Environment launch MSYS2 with: 

msys2

In the MSYS2 environment type:

$ cd ~		<== ensure you at home
$ mkdir builds
$ cd builds


4. Locate and unzip LibUSB matching version deployed above into builds
======================================================================

** These steps in the original build-w64.sh script are redundant **

** You should familiarise yourself with these steps - but may skip this **

JTSDK 3.2.0 (and later) points the environment to the libusb 
deployment in X:\JTSDK64-Tools\tools\libusb through 
environment variable $libusb_dir_f .

If you need the source for LibUSB use steps similar to those
below to obtain source:  

$ wget https://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.24/libusb-1.0.24.tar.bz2
$ tar -xvf libusb-1.0.24.tar.bz2

[ This creates a subdirectory libusb-1.0.24 ]


5. Obtain latest Hamlib Source from Github
==========================================

$ git clone https://github.com/Hamlib/Hamlib.git hamlib-4.2~git


6. Start the Process Rolling
============================

$ cd ./hamlib-4.2~git
$ ./bootstrap			       
$ ./scripts/build-w64-jtsdk.sh hamlib-4.2~git


7. Tadaa - Drumroll !
=====================

==> Package ......... ~/build/hamlib-4.2~git/hamlib-w64-4.2~git ==> hamlib-w64-4.2~git.zip
==> Headers ......... ~/build/hamlib-4.2~git/hamlib-w64-4.2~git/include
==> Library &tools .. ~/build/hamlib-4.2~git/hamlib-w64-4.2~git/bin

