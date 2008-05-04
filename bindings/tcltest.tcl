#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

load ".libs/hamlibtcl.so" Hamlib
#package require Hamlib

set tclver [info tclversion]
puts "Tcl $tclver test, $hamlib_version\n"

#rig_set_debug $RIG_DEBUG_TRACE
rig_set_debug $RIG_DEBUG_NONE

# Init RIG_MODEL_DUMMY
Rig my_rig $RIG_MODEL_DUMMY

my_rig open
my_rig set_freq 145550000

puts status:[my_rig cget -error_status]

# get_mode returns a tuple
set moderes [my_rig get_mode]
set mode [rig_strrmode [lindex $moderes 0]]
puts "mode: $mode, bandwidth:[lindex $moderes 1]Hz"

set state [my_rig cget -state]
puts ITU_region:[$state cget -itu_region]

# The following works well also
# puts ITU_region:[[my_rig cget -state] cget -itu_region]

puts getinfo:[my_rig get_info]

my_rig set_level "VOX"  1
puts status:[my_rig cget -error_status]
puts "VOX level:[my_rig get_level_i "VOX"]"
puts status:[my_rig cget -error_status]
my_rig set_level $RIG_LEVEL_VOX 5
puts status:[my_rig cget -error_status]
puts "VOX level:[my_rig get_level_i $RIG_LEVEL_VOX]"
puts status:[my_rig cget -error_status]

puts strength:[my_rig get_level_i $RIG_LEVEL_STRENGTH]
puts status:[my_rig cget -error_status]
puts status(str):[rigerror [my_rig cget -error_status]]

my_rig close
#my_rig cleanup


exit 0
