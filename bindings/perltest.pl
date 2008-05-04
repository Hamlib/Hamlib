#!/usr/bin/perl -Iblib/arch -Iblib/lib

use Hamlib;

print "Perl $] test, version: $Hamlib::hamlib_version\n";

#Hamlib::rig_set_debug($Hamlib::RIG_DEBUG_TRACE);
Hamlib::rig_set_debug($Hamlib::RIG_DEBUG_NONE);

$rig = new Hamlib::Rig($Hamlib::RIG_MODEL_DUMMY);

# replace "/dev/Rig" with your path to serial port
$rig->set_conf("retry","50");
$rig->set_conf("rig_pathname","/dev/Rig");

$rig->open();

# 1073741944 is token value for "itu_region"
# but using get_conf is much more convenient
$region = $rig->get_conf(1073741944);
$rpath = $rig->get_conf("rig_pathname");
$retry = $rig->get_conf("retry");
print "get_conf: path=\"$rpath\", retry=$retry, ITU region=$region\n";


$rig->set_freq(14266000, $Hamlib::RIG_VFO_A);

$f = $rig->get_freq();
print "freq: $f\n";

($mode, $width) = $rig->get_mode();
print "get_mode: ".Hamlib::rig_strrmode($mode).", width: $width\n";

$vfo = $rig->get_vfo();
print "get_vfo: ".Hamlib::rig_strvfo($vfo)."\n";
$rig->set_vfo($Hamlib::RIG_VFO_B);
#$rig->set_vfo("VFOA");

$rig->set_mode($Hamlib::RIG_MODE_CW, $Hamlib::RIG_PASSBAND_NORMAL);

print "ITU region: $rig->{state}->{itu_region}\n";
print "Backend copyright: $rig->{caps}->{copyright}\n";
$inf = $rig->get_info();

print "get_info: $inf\n";

$rig->set_level("VOX", 1);
$lvl = $rig->get_level_i("VOX");
print "VOX level: $lvl\n";
$rig->set_level($Hamlib::RIG_LEVEL_VOX, 5);
$lvl = $rig->get_level_i($Hamlib::RIG_LEVEL_VOX);
print "VOX level: $lvl\n";

$lvl = $rig->get_level_i($Hamlib::RIG_LEVEL_STRENGTH);
print "strength: $lvl\n";


$chan = new Hamlib::channel($Hamlib::RIG_VFO_A);

$rig->get_channel($chan);
print "get_channel status: $rig->{error_status} = ".Hamlib::rigerror($rig->{error_status})."\n";

print "VFO: ".Hamlib::rig_strvfo($chan->{vfo}).", $chan->{freq}\n";

$rig->close();


print "\nSome static functions:\n";

($err, $long1, $lat1, $sw1) = Hamlib::locator2longlat("IN98EC");
($err, $long2, $lat2, $sw1) = Hamlib::locator2longlat("DM33DX");
$loc1 = Hamlib::longlat2locator($long1, $lat1, 3);
$loc2 = Hamlib::longlat2locator($long2, $lat2, 3);
print "Loc1: $loc1\n";
print "Loc2: $loc2\n";

($err, $dist, $az) = Hamlib::qrb($long1, $lat1, $long2, $lat2);
$longpath = Hamlib::distance_long_path($dist);
print "Distance: $dist km, azimuth $az, long path: $longpath\n";
($err, $deg, $min, $sec, $sw) = Hamlib::dec2dms($az);
$deg = -$deg if ($sw == 1);
$az2 = Hamlib::dms2dec($deg, $min, $sec, $sw);
print "Bearing: $az, $deg° $min' $sec\", recoded: $az2\n"


