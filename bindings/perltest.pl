#!/usr/bin/perl -Iblib/arch -Iblib/lib

use Hamlib;

print "Perl $] test, version: $Hamlib::hamlib_version\n\n";

#Hamlib::rig_set_debug($Hamlib::RIG_DEBUG_TRACE);
Hamlib::rig_set_debug($Hamlib::RIG_DEBUG_NONE);

$rig = new Hamlib::Rig($Hamlib::RIG_MODEL_DUMMY);

# replace "/dev/Rig" with your path to serial port
$rig->set_conf("retry","50");
$rig->set_conf("rig_pathname","/dev/Rig");

$rig->open();

$rpath = $rig->get_conf("rig_pathname");
$retry = $rig->get_conf("retry");
print "get_conf:\t\tpath = \"$rpath\", retry = $retry\n";


$rig->set_freq($Hamlib::RIG_VFO_A, 14266000);

$f = $rig->get_freq();
print "freq:\t\t\t$f\n";

($mode, $width) = $rig->get_mode();
print "get_mode:\t\t".Hamlib::rig_strrmode($mode)."\nwidth:\t\t\t$width\n";

$vfo = $rig->get_vfo();
print "get_vfo:\t\t".Hamlib::rig_strvfo($vfo)."\n";
$rig->set_vfo($Hamlib::RIG_VFO_B);
#$rig->set_vfo("VFOA");

$rig->set_mode($Hamlib::RIG_MODE_CW, $Hamlib::RIG_PASSBAND_NORMAL);

print "Backend copyright:\t$rig->{caps}->{copyright}\n";
print "Model:\t\t\t$rig->{caps}->{model_name}\n";
print "Manufacturer:\t\t$rig->{caps}->{mfg_name}\n";
print "Backend version:\t$rig->{caps}->{version}\n";
$inf = $rig->get_info();

print "get_info:\t\t$inf\n";

$rig->set_level("VOX", 1);
$lvl = $rig->get_level_i("VOX");
print "VOX delay:\t\t$lvl\n";
$rig->set_level($Hamlib::RIG_LEVEL_VOXDELAY, 5);
$lvl = $rig->get_level_i($Hamlib::RIG_LEVEL_VOXDELAY);
print "VOX delay:\t\t$lvl\n";

$lvl = $rig->get_level_i($Hamlib::RIG_LEVEL_STRENGTH);
print "strength:\t\t$lvl\n";


$chan = new Hamlib::channel($Hamlib::RIG_VFO_A);

$rig->get_channel($chan,1);
@tokens = split("\n",Hamlib::rigerror($rig->{error_status}));
print "get_channel status:\t$rig->{error_status} = ".$tokens[0]."\n";

print "VFO:\t\t\t".Hamlib::rig_strvfo($chan->{vfo}).", $chan->{freq}\n";

$att = $rig->{caps}->{attenuator};
print "Attenuators:\t\t@$att\n";

print "\nSending Morse, '73'\n";
$rig->send_morse($Hamlib::RIG_VFO_A, "73");

$rig->close();


print "\nSome static functions:\n";

($err, $lon1, $lat1, $sw1) = Hamlib::locator2longlat("IN98XC");
($err, $lon2, $lat2, $sw1) = Hamlib::locator2longlat("DM33DX");
$loc1 = Hamlib::longlat2locator($lon1, $lat1, 3);
$loc2 = Hamlib::longlat2locator($lon2, $lat2, 3);
printf("Loc1:\t\tIN98XC -> %9.4f, %9.4f-> %s\n", $lon1, $lat1, $loc1);
printf("Loc1:\t\tDM33DX -> %9.4f, %9.4f-> %s\n", $lon2, $lat2, $loc2);

($err, $dist, $az) = Hamlib::qrb($lon1, $lat1, $lon2, $lat2);
$longpath = Hamlib::distance_long_path($dist);

printf("Distance:\t%.3f km, azimuth %.2f, long path: %.3f km\n",
	$dist, $az, $longpath);

# dec2dms expects values from 180 to -180
# sw is 1 when deg is negative (west or south) as 0 cannot be signed
($err, $deg1, $min1, $sec1, $sw1) = Hamlib::dec2dms($lon1);
($err, $deg2, $min2, $sec2, $sw2) = Hamlib::dec2dms($lat1);

$lon3 = Hamlib::dms2dec($deg1, $min1, $sec1, $sw1);
$lat3 = Hamlib::dms2dec($deg2, $min2, $sec2, $sw2);

printf("Longitude:\t%9.4f, %4d° %2d' %2d\" %1s\trecoded: %9.4f\n",
	$lon1, $deg1, $min1, $sec1, $sw1 ? 'W' : 'E', $lon3);

printf("Latitude:\t%9.4f, %4d° %2d' %2d\" %1s\trecoded: %9.4f\n",
	$lat1, $deg2, $min2, $sec2, $sw2 ? 'S' : 'N', $lat3);
