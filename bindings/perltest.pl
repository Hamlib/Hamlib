#!/usr/bin/perl

# make sure the module is there, hint: "ln -s .lib/hamlibperl.so hamlib.so"
use hamlib;

print "Version: $hamlib::hamlib_version\n";
print "FM: $hamlib::RIG_MODE_FM\n";

hamlib::rig_set_debug($hamlib::RIG_DEBUG_TRACE);

$rig = new hamlib::Rig($hamlib::RIG_MODEL_DUMMY);
$rig->open();

# 1073741944 is token value for "itu_region"
$rpath = $rig->get_conf("rig_pathname");
$region = $rig->get_conf(1073741944);
print "get_conf: path=\"$rpath\", ITU region=$region\n";


$rig->set_freq(12000000, $hamlib::RIG_VFO_A);

$f = $rig->get_freq();
print "freq: $f\n";

($mode, $width) = $rig->get_mode();

print "mode: $mode, width: $width\n";

print "ITU region: $rig->{state}->{itu_region}\n";
print "Copyright: $rig->{caps}->{copyright}\n";
$inf = $rig->get_info();

print "get_info: $inf\n";

$rig->set_level("VOX", 1);
$lvl = $rig->get_level_i("VOX");
print "level: $lvl\n";
$rig->set_level($hamlib::RIG_LEVEL_VOX, 5);
$lvl = $rig->get_level_i($hamlib::RIG_LEVEL_VOX);
print "level: $lvl\n";



$chan = new hamlib::Chan($hamlib::RIG_VFO_A);

$rig->get_channel($chan);
print "get_channel status: $rig->{error_status}\n";

print "VFO: $chan->{vfo}, $chan->{freq}\n";

$rig->close();

# TODO:
($long1, $lat1) = hamlib::locator2longlat("IN98EC");
($long2, $lat2) = hamlib::locator2longlat("DM33DX");
$loc1 = hamlib::longlat2locator($long1, $lat1);
$loc2 = hamlib::longlat2locator($long2, $lat2);
print "Loc1: $loc1\n";
print "Loc2: $loc2\n";

($dist, $az) = hamlib::qrb($long1, $lat1, $long2, $lat2);
$longpath = hamlib::distance_long_path($dist);
print "Distance: $dist km, long path: $longpath\n";
($deg, $min, $sec) = hamlib::dec2dms($az);
$az2 = hamlib::dms2dec($deg, $min, $sec);
print "Bearing: $az, $deg° $min' $sec\", recoded: $az2\n"


