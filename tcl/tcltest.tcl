#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

load "../tcl/.libs/libhamlibtcl.so" Hamlib
#load "./hamlibtcl.so" Hamlib

puts [rig version]\n

# Init RIG_MODEL_DUMMY
set my_rig [rig init 1]
$my_rig open
$my_rig set_freq 145550000
puts [$my_rig get_strength]
$my_rig close
$my_rig cleanup


exit 0
