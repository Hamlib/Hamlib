#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

load ".libs/hamlibtcl.so" Hamlib

puts $hamlib_version\n

# Init RIG_MODEL_DUMMY
Rig my_rig $RIG_MODEL_DUMMY

my_rig open
my_rig set_freq 145550000

# note: get_level may not be implemented yet!
puts [my_rig get_level $RIG_LEVEL_STRENGTH]
my_rig close
my_rig cleanup


exit 0
