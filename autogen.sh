#!/bin/sh

# Run this to generate all the initial makefiles, etc.

# Enabling maintainer mode, as done when configure is run by this script,
# will cause the makefiles to be regenerated if any of the Makefile.am
# or configure.ac files are changed.

# Taken from glib CVS

# Change the following to match the versions installed on your system
LIBTOOLIZE=libtoolize
AUTORECONF=autoreconf
AUTOMAKE=automake

# variables below this line should not need modification
SRCDIR=`dirname $0`
test -z "$SRCDIR" && SRCDIR=.

ORIGDIR=`pwd`

PROJECT=hamlib

TEST_TYPE=-f
FILE=include/hamlib/rig.h

DIE=0

($AUTORECONF --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autoreconf installed to compile $PROJECT."
        echo "Download the appropriate package for your distribution,"
        DIE=1
}

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have automake installed to compile $PROJECT."
        echo "Download the appropriate package for your distribution,"
        DIE=1
}

($LIBTOOLIZE --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have libtool installed to compile $PROJECT."
        echo "Download the appropriate package for your distribution."
        DIE=1
}

if test "$DIE" -eq 1; then
        exit 1
fi

cd $SRCDIR

test $TEST_TYPE $FILE || {
        echo "You must run this script in the top-level $PROJECT directory"
        exit 1
}

###################################################################
### autoreconf is now the preferred way to process configure.ac ###
### which should handle compiler variations and ensures that    ###
### subtools are processed in the correct order.                ###
###################################################################

echo "Running '$AUTORECONF -i' to process configure.ac"
echo "and generate the configure script."

# Tell autoreconf to install needed build system files
$AUTORECONF -i

cd $ORIGDIR

if test -z "$*"; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

$SRCDIR/configure "$@"
