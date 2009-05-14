#!/bin/sh

# Discover the version number of libtool on this system.
# and set up autogen.sh and configure.ac accordingly.

# Run this script after downloading source from SVN and before
# running autogen.sh to ensure correct libtool operation.

# Version 1 files
V1AUTOGEN='autogen.sh.ltv1'
V1CONFIG='configure.ac.ltv1'

# Version 2 (and later?) files
V2AUTOGEN='autogen.sh.ltv2'
V2CONFIG='configure.ac.ltv2'

# Destinations
AUTOGEN='autogen.sh'
CONFIG='configure.ac'

TMPFILE=/tmp/libtoolchk$$

if [ ! -e $AUTOGEN ]; then
	echo File $AUTOGEN not detected.
	echo This procedure is needed only if you are working with source
	echo from an SVN checkout, where autogen.sh is provided. Exiting.
	exit 1
fi
echo $(libtool --version) | \
    sed '1,1s/ltmain.*tool) //; 1,1s/ .*$//; 2,$d' > $TMPFILE

# Test first digit of version.  If it's '1', use libtool v1 setup, otherwise
# use libtool v2.

vers=$(cat $TMPFILE)
echo Libtool version $vers detected.

if [ $vers \< "1.99.99" ]; then 
	cp $V1AUTOGEN $AUTOGEN
	cp $V1CONFIG  $CONFIG
	echo Libtool v1 configured.
   else 
	cp $V2AUTOGEN $AUTOGEN
	cp $V2CONFIG  $CONFIG
	echo Libtool v2 configured
fi

rm $TMPFILE

