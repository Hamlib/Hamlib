#!/usr/bin/perl

# make sure the module is there, hint: "ln -s .lib/hamlibperl.so hamlib.so"
use hamlib;

print "Version: $hamlib::hamlib_version\n";
print "FM: $hamlib::RIG_MODE_FM\n";

hamlib::rig_set_debug(5);

$rig = new hamlib::Rig($hamlib::RIG_MODEL_DUMMY);
$rig->open();

$rig->set_freq(12000000, $hamlib::RIG_VFO_A);

print "ITU region: $rig->{state}->{itu_region}\n";
print "Copyright: $rig->{caps}->{copyright}\n";

$rig->close();
