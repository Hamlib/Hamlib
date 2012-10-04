# NSIS script to build a Win32 installer for Hamlib

# Portions of this script obtained from NSIS examples and other documentation.
# No copyright claim is made on this script by Nate Bargmann, N0NB,
# nor the Hamlib Group.  This script is made publicly available in the hope
# that it will be useful to others.

#--------------------------------
# Installer Attributes
#--------------------------------

# Use the command line option `makensis -DRELEASE=3.0 hamlib.nsi`
# instead of a static definition.
!define HAMLIB hamlib-win32-${RELEASE}

# Compile this script from outside the Hamlib tree, often as
# a sibling directory of the Hamlib build directory works well.
!define SOURCE_BIN hamlib-${RELEASE}\${HAMLIB}

# LICENSE.txt is a summary rather than inclusion
# of the entire GPL and LGPL texts.
!define FILE_LICENSE LICENSE.txt

# Name of installer
Name "${HAMLIB}"

# The file to write
OutFile "${HAMLIB}.exe"

# The default installation directory on Win32,
# defaulting to 'C:\Program Files\hamlib-win32-x.x'
InstallDir "$PROGRAMFILES\${HAMLIB}"

# License text and properties
LicenseData "..\${SOURCE_BIN}\${FILE_LICENSE}"
LicenseBkColor /windows

# Request application privileges for Windows Vista and later when UAC is turned on.
RequestExecutionLevel user

#--------------------------------
# Pages
#--------------------------------

Page license
Page components
Page directory
Page instfiles

# UninstPage uninstconfirm
# UninstPage instfiles

#--------------------------------
# Sections
#--------------------------------
# Output path is set prior to each file statement so the files end up in
# the desired directory.

# DLLs are required for either installation option.  Also include the text
# files as required.
Section "DLLs (required)"
  # Files installed in this section cannot be deselected by the user
  SectionIn RO

  SetOutPath $INSTDIR\bin
  File /r ..\${SOURCE_BIN}\bin\*.dll

  SetOutPath $INSTDIR
  File /r ..\${SOURCE_BIN}\*.txt

SectionEnd

# Install the user programs and their documentation
Section "Programs"
  # Enabled by default, installation of these files is optional.

  SetOutPath $INSTDIR\bin
  File /r ..\${SOURCE_BIN}\bin\*.exe

  SetOutPath $INSTDIR\pdf
  File /r ..\${SOURCE_BIN}\pdf\*.*

SectionEnd

# Install the header files and GCC and MSVCC development libraries
Section "Development"
  # Enabled by default, installation of these files is optional.

  SetOutPath $INSTDIR\include
  File /r ..\${SOURCE_BIN}\include\*.*

  SetOutPath $INSTDIR\lib
  File /r ..\${SOURCE_BIN}\lib\*.*

SectionEnd

#--------------------------------
# Functions
#--------------------------------
Function .onInit
  MessageBox MB_YESNO "This will install ${HAMLIB}.$\nDo you want to continue?" IDYES startnow
    Abort
  startnow:
FunctionEnd
