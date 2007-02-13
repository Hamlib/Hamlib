#!/bin/sh
# Run this to generate or regenerate all the initial makefiles, etc.
# Taken from glib CVS

# Change the following to match the versions installed on your system
AUTOCONF=autoconf
AUTOHEADER=autoheader
AUTOMAKE=automake
ACLOCAL=aclocal
#
# FreeBSD 6.2 uses this
#
AUTOCONF=autoconf259
AUTOHEAD=autoheader259
AUTOMAKE=automake19
ACLOCAL=aclocal19
#
# Debian etch uses this
#
#AUTOCONF=autoconf
#AUTOHEADER=autoheader
#AUTOMAKE=automake-1.9
#ACLOCAL=aclocal-1.9

# Needed on Gentoo
export WANT_AUTOCONF_2_5	#  2.54 or higher, not 2.53a or 2.13

ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I macros"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=hamlib
TEST_TYPE=-f
FILE=include/hamlib/rig.h

DIE=0

($AUTOCONF --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autoconf installed to compile $PROJECT."
        echo "Download the appropriate package for your distribution,"
        echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
        DIE=1
}

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have automake installed to compile $PROJECT."
        echo "Get ftp://sourceware.cygnus.com/pub/automake/automake-1.5.tar.gz"
        echo "(or a newer version if it is available)"
        DIE=1
}

if test "$DIE" -eq 1; then
        exit 1
fi

test $TEST_TYPE $FILE || {
        echo "You must run this script in the top-level $PROJECT directory"
        exit 1
}

if test -z "$*"; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

# Are we looking for the compiler on a foreign system?
case $CC in
*xlc | *xlc\ * | *lcc | *lcc\ *) am_opt=--include-deps;;
esac

$ACLOCAL $ACLOCAL_FLAGS

# optionally feature autoheader
($AUTOHEADER --version)  < /dev/null > /dev/null 2>&1 && $AUTOHEADER

libtoolize --automake --ltdl
$AUTOMAKE -a $am_opt
$AUTOCONF
cd $ORIGDIR

$srcdir/configure --enable-maintainer-mode "$@"
