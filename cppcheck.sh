#!/bin/sh

# Author Michael Black W9MDB
# This SUPPRESS setting results in no warnings as of 2020-01-14
# There are things that could still be done...especially in the C++ area
echo "Ideally there should be no errors or warnings"

# We do suppress some errors which are expected or other code
# There are quite a few C++ items to take care of still if anybody cares
SUPPRESS="\
-i c++/rigclass.cc \
-i c++/rotclass.cc \
-i c++/ampclass.cc \
-i bindings \
-i lib/getopt.c \
-i lib/getopt_long.c \
-i lib/cJSON.c \
--suppress=*:extra/gnuradio/demod.h \
--suppress=*:extra/gnuradio/HrAGC.h \
--suppress=*:extra/gnuradio/nfm.h \
--suppress=*:extra/gnuradio/am.h \
--suppress=*:extra/gnuradio/ssb.h \
--suppress=*:extra/gnuradio/wfm.h \
--suppress=*:extra/gnuradio/wfm.h \
--suppress=*:extra/gnuradio/HrAGC.h \
--suppress=*:extra/gnuradio/gnuradio.cc \
--suppress=missingIncludeSystem \
--suppress=*:style/rigs/adat/adat.c

#CHECK="\
#-D RIG_LEVEL_LINEOUT=1 \
#-D SIGPIPE \
#-D SIGINT \
# -D IPV6_V6ONLY \
# -D RIG_MODE_WFM \
# -D ABI_VERSION=4 \
# -D F_SETSIG=1 \
# -U O_ASYNC \
# -U SA_SIGINFO \
# -U HASH_BLOOM \
# -U HASH_EMIT_KEYS \
# -U HASH_FUNCTION \
# -U __USEP5P6__"

CHECK="\
-Duint64_t \
-D HAMLIB_EXPORT \
-D HAMLIB_EXPORT_VAR \
-D __WORDSIZE \
-D BACKEND_EXPORT \
-D PRId64 \
-D DECLARE_INITRIG_BACKEND \
-D DECLARE_INITRROT_BACKEND \
-D DECLARE_INITAMP_BACKEND \
-D B230400
-U RIG_LEVEL_LINEOUT \
-U O_ASYNC \
-U F_SETSIG \
-U SA_SIGINFO \
-U SIGPIPE \
-U gai_strerror \
-U CMSPAR \
-U TIOCCBRK \
-U TIOCSBRK \
-U TIOCMBIC \
-U TIOCMBIS \
-U HASH_BLOOM \
-U HASH_EMIT_KEYS \
-U HASH_FUNCTION \
-U IPV6_V6ONLY \
-D SIGINT \
-D WIN32 \
-D CLOCK_REALTIME \
-D HAVE_PTHREAD \
-D HAVE_SIGNAL"

# If no directory or file name provided, scan the entire project.
if test $# -eq 0 ; then
        echo "See cppcheck.log when done"
        echo "This takes a while to run"
        cppcheck --inline-suppr \
                 --check-level=exhaustive \
                 -I src \
                 -I include \
                 -I include/hamlib/ \
                 -I lib \
                 -I security \
                 -q \
                 --force \
                 --enable=all \
                 --std=c99 \
                 $SUPPRESS \
                 $CHECK \
                 --template='{file}:{line},{severity},{id},{message}' \
                 . \
                 >cppcheck.log 2>&1
else
        cppcheck --inline-suppr \
                 --check-level=exhaustive
                 -I src \
                 -I include \
                 -I include/hamlib/ \
                 -I lib \
                 -I security \
                 -q \
                 --force \
                 --enable=all \
                 --std=c99 \
                 $SUPPRESS \
                 $CHECK \
                 --template='{file}:{line},{severity},{id},{message}'\
                 "$@"
fi
