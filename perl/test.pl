# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test;
BEGIN { plan tests => 12 };
use Hamlib;
ok(1); # If we made it this far, we're ok.

#########################

# Insert your test code below, the Test module is use()ed here so read
# its man page ( perldoc Test ) for help writing this test script.

# 1 => RIG_MODEL_DUMMY

$rig = Hamlib::Rig::init(1);
ok(defined $rig);

# set configuration parameter and readback
$rig->set_conf("itu_region", "1");
ok($rig->get_conf("itu_region") eq "1");

# open rig communication
$rig->open();
ok(1);

# Get some info from the rig, is available
print "Info: " . $rig->get_info() . "\n";
ok(1);

# Here's the plan. Select VFO A, check we got it.
# Then tune to 50.110 MHz, and set USB mode.

$rig->set_vfo(RIG_VFO_A);
ok(1);

ok($rig->get_vfo() == RIG_VFO_A);

$rig->set_freq(50110000);
ok(1);

ok($rig->get_freq() == 50110000);

$rig->set_mode(RIG_MODE_USB);
ok(1);

my ($mode, $width) = $rig->get_mode();
ok($mode == RIG_MODE_USB);

# CQ CQ ...


# No propagation today on 6. Maybe tomorrow.
$rig->close();
ok(1);

# NB: destroy done automatically

